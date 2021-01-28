// Copyright 2020 Google LLC
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//      http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#include "agent_based_epidemic_sim/core/risk_score.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstddef>
#include <memory>

#include "absl/flags/declare.h"
#include "absl/flags/flag.h"
#include "absl/memory/memory.h"
#include "absl/random/distributions.h"
#include "absl/status/status.h"
#include "absl/status/statusor.h"
#include "absl/strings/str_cat.h"
#include "absl/time/time.h"
#include "absl/types/optional.h"
#include "agent_based_epidemic_sim/applications/risk_learning/config.pb.h"
#include "agent_based_epidemic_sim/applications/risk_learning/risk_score.h"
#include "agent_based_epidemic_sim/core/constants.h"
#include "agent_based_epidemic_sim/core/event.h"
#include "agent_based_epidemic_sim/core/integral_types.h"
#include "agent_based_epidemic_sim/core/location_type.h"
#include "agent_based_epidemic_sim/core/pandemic.pb.h"
#include "agent_based_epidemic_sim/core/random.h"
#include "agent_based_epidemic_sim/core/risk_score_model.h"
#include "agent_based_epidemic_sim/port/deps/status_macros.h"
#include "agent_based_epidemic_sim/port/time_proto_util.h"
#include "agent_based_epidemic_sim/util/time_utils.h"

ABSL_FLAG(bool, request_test_using_hazard, false,
          "If true, with probability hazard request a test.");
ABSL_DECLARE_FLAG(bool, request_test_using_hazard);

namespace abesim {

namespace {

struct TracingPolicy {
  absl::Duration quarantine_duration_contacts;
  absl::Duration quarantine_duration_risk_score;
  absl::Duration quarantine_duration_symptoms;
  absl::Duration quarantine_duration_positive;
  float quarantine_risk_score_threshold;
  bool quarantine_on_symptoms;

  absl::Duration test_validity_duration;
  absl::Duration test_latency;
  float test_sensitivity;
  float test_specificity;
  bool test_on_symptoms;
  float test_risk_score_threshold;
  bool test_all_per_timestep;
  bool test_on_contact;

  absl::Duration contact_retention_duration;
  bool trace_on_positive;
  float traceable_interaction_fraction;
};

bool IsSymptomatic(const HealthState::State health_state) {
  return health_state != HealthState::EXPOSED &&
         health_state != HealthState::ASYMPTOMATIC &&
         health_state != HealthState::PRE_SYMPTOMATIC_MILD &&
         health_state != HealthState::PRE_SYMPTOMATIC_SEVERE &&
         health_state != HealthState::RECOVERED;
}

// NB: Timesteps are non-overlapping, so checking start_time ordering
// suffices.
struct TimestepComparator {
  bool operator()(const Timestep& a, const Timestep& b) const {
    return a.start_time() < b.start_time();
  }
};

// A policy that implements testing, tracing, and isolation guidelines.
class LearningRiskScore : public RiskScore {
 public:
  LearningRiskScore(const TracingPolicy& tracing_policy,
                    const RiskScoreModel* risk_score_model,
                    const LearningRiskScorePolicy& risk_score_policy,
                    LocationTypeFn location_type)
      : tracing_policy_(tracing_policy),
        risk_score_model_(*risk_score_model),
        risk_score_policy_(risk_score_policy),
        location_type_(std::move(location_type)),
        infection_onset_time_(absl::InfiniteFuture()),
        latest_symptom_time_(absl::InfinitePast()),
        latest_contact_time_(absl::InfinitePast()),
        risk_score_per_timestep_(
            risk_score_policy.exposure_notification_window_days) {}

  void AddHealthStateTransistion(HealthTransition transition) override {
    // QUESTION: Should this exclude EXPOSED?
    if (transition.health_state != HealthState::SUSCEPTIBLE) {
      infection_onset_time_ = std::min(infection_onset_time_, transition.time);
      if (IsSymptomatic(transition.health_state)) {
        latest_symptom_time_ = std::max(latest_symptom_time_, transition.time);
        if (tracing_policy_.test_on_symptoms &&
            !HasActiveTest(latest_symptom_time_)) {
          RequestTest(latest_symptom_time_);
        }
      }
    }
  }
  // TODO: Remove this interface and handle logic in
  // AddExposureNotification. latest_timestep_ is no longer used.
  void UpdateLatestTimestep(const Timestep& timestep) override {
    // Assume this is called each timestep for each agent.
    latest_timestep_ = timestep;
    // Garbage collect.
    for (auto iter = timestep_to_id_.begin();
         head_ != tail_ && iter != timestep_to_id_.end();) {
      if (iter->first.start_time() >=
          timestep.start_time() - ContactRetentionDuration()) {
        break;
      }
      GetRiskScorePerTimestepById(iter->second) = 0;
      head_ = (head_ + 1) % risk_score_per_timestep_.size();
      head_id_++;
      iter = timestep_to_id_.erase(iter);
    }
    const size_t current = size();
    if (current + 1 >= risk_score_per_timestep_.size()) {
      // Reallocate buffer.
      std::vector<float> tmp(risk_score_per_timestep_.size() * 2);
      size_t id = head_id_;
      for (int i = 0; i < current; ++i) {
        tmp[i] = std::move(GetRiskScorePerTimestepById(id++));
      }
      std::swap(risk_score_per_timestep_, tmp);
      head_ = 0;
      tail_ = current;
    }
    tail_ = (tail_ + 1) % risk_score_per_timestep_.size();
    size_t next_id = head_id_ + current;
    GetRiskScorePerTimestepById(next_id) = 0.0f;
    timestep_to_id_.insert({timestep, next_id});

    if (tracing_policy_.test_all_per_timestep) {
      RequestTest(timestep.start_time());
    }
  }

  TestOutcome::Outcome GetOutcome(const absl::Time request_time) {
    if (request_time >= infection_onset_time_) {
      // Actual positive.
      return absl::Bernoulli(GetBitGen(), tracing_policy_.test_sensitivity)
                 ? TestOutcome::POSITIVE
                 : TestOutcome::NEGATIVE;
    }
    // Actual negative.
    return absl::Bernoulli(GetBitGen(), tracing_policy_.test_specificity)
               ? TestOutcome::NEGATIVE
               : TestOutcome::POSITIVE;
  }

  void AddExposureNotification(const Exposure& exposure,
                               const ContactReport& notification) override {
    if (!absl::Bernoulli(GetBitGen(),
                         tracing_policy_.traceable_interaction_fraction)) {
      return;
    }
    // Actuate based on app user flag.
    // We don't take action on negative tests.
    if (notification.test_result.outcome != TestOutcome::POSITIVE) return;
    float risk_score = risk_score_model_.ComputeRiskScore(
        exposure, notification.initial_symptom_onset_time);
    VLOG(1) << "Risk score is (" << risk_score
            << ") for exposure: " << exposure;
    auto status = AppendRiskScoreForExposure(risk_score, exposure);
    if (!status.ok()) LOG(ERROR) << status;

    absl::Time new_contact_time = exposure.start_time + exposure.duration;
    // If we already know about a contact that happened after this new
    // notification, we aren't going to take action based on this.
    if (latest_contact_time_ >= new_contact_time) return;

    latest_contact_time_ = new_contact_time;

    absl::Time request_time = notification.test_result.time_received;

    // This means that if we receive an exposure notification for a contact
    // that happened within test_validity_duration of the last time we started
    // a test, we ignore that exposure.
    if (HasActiveTest(request_time)) return;

    if (GetProbabilisticRiskScore() >=
        tracing_policy_.test_risk_score_threshold) {
      RequestTest(request_time);
    } else if (tracing_policy_.test_on_contact) {
      RequestTest(request_time);
    }
  }

  VisitAdjustment GetVisitAdjustment(const Timestep& timestep,
                                     const int64 location_uuid) const override {
    const bool skip_visit =
        location_type_(location_uuid) != LocationReference::HOUSEHOLD &&
        (ShouldQuarantineFromContacts(timestep) ||
         ShouldQuarantineFromSymptoms(timestep) ||
         ShouldQuarantineFromPositive(timestep) ||
         ShouldQuarantineFromRiskScore());
    return {
        .frequency_adjustment = skip_visit ? 0.0f : 1.0f,
        .duration_adjustment = 1.0f,
    };
  }
  TestResult GetTestResult(const Timestep& timestep) const override {
    for (auto result = test_results_.rbegin(); result != test_results_.rend();
         ++result) {
      if (result->time_received < timestep.end_time()) return *result;
    }
    return {.time_requested = absl::InfiniteFuture(),
            .time_received = absl::InfiniteFuture(),
            .outcome = TestOutcome::UNSPECIFIED_TEST_RESULT};
  }

  ContactTracingPolicy GetContactTracingPolicy(
      const Timestep& timestep) const override {
    // Actuate based on app user flag.
    TestResult result = GetTestResult(timestep);
    bool should_report =
        tracing_policy_.trace_on_positive &&
        result.outcome == TestOutcome::POSITIVE &&
        result.time_received <= timestep.end_time() &&
        result.time_requested + tracing_policy_.contact_retention_duration >=
            timestep.start_time();
    return {.report_recursively = false, .send_report = should_report};
  }

  absl::Duration ContactRetentionDuration() const override {
    return tracing_policy_.contact_retention_duration;
  }

  // Gets the current risk score of the associated agent.
  float GetRiskScore() const override {
    // NB: Because the risk score is garbage collected over the relevant
    // window of time every time UpdateLatestTimestep is called, we sum
    // over the contents of the risk buffer.
    return std::accumulate(risk_score_per_timestep_.begin(),
                           risk_score_per_timestep_.end(), 0.0f);
  }

  void RequestTest(const absl::Time request_time) {
    test_results_.push_back({
        .time_requested = request_time,
        .time_received = request_time + tracing_policy_.test_latency,
        .outcome = GetOutcome(request_time),
    });
  }

 private:
  bool HasActiveTest(absl::Time request_time) const {
    return HasTestResults() &&
           (HasPositiveTest(request_time) || HasValidTest(request_time));
  }
  bool HasTestResults() const { return !test_results_.empty(); }
  bool HasPositiveTest(absl::Time query_time) const {
    return test_results_.back().outcome == TestOutcome::POSITIVE &&
           test_results_.back().time_received <= query_time;
  }
  bool HasValidTest(absl::Time request_time) const {
    return test_results_.back().time_requested +
               tracing_policy_.test_validity_duration >
           request_time;
  }

  bool ShouldQuarantineFromContacts(const Timestep& timestep) const {
    absl::Time earliest_quarantine_time =
        std::min(timestep.start_time() - ContactRetentionDuration(),
                 latest_contact_time_);
    absl::Time latest_quarantine_time =
        latest_contact_time_ + tracing_policy_.quarantine_duration_contacts;
    return (timestep.start_time() < latest_quarantine_time &&
            timestep.end_time() > earliest_quarantine_time);
  }

  bool ShouldQuarantineFromRiskScore() const {
    return GetProbabilisticRiskScore() >
           tracing_policy_.quarantine_risk_score_threshold;
  }

  bool ShouldQuarantineFromSymptoms(const Timestep& timestep) const {
    if (!tracing_policy_.quarantine_on_symptoms) return false;
    absl::Time earliest_quarantine_time = std::min(
        timestep.start_time() - tracing_policy_.quarantine_duration_symptoms,
        latest_symptom_time_);
    absl::Time latest_quarantine_time =
        latest_symptom_time_ + tracing_policy_.quarantine_duration_symptoms;
    return (timestep.start_time() < latest_quarantine_time) &&
           (timestep.end_time() > earliest_quarantine_time);
  }

  bool ShouldQuarantineFromPositive(const Timestep& timestep) const {
    if (!HasTestResults() || !HasPositiveTest(timestep.start_time()) ||
        tracing_policy_.quarantine_duration_positive == absl::ZeroDuration()) {
      return false;
    }
    absl::Time earliest_quarantine_time = std::min(
        timestep.start_time() - tracing_policy_.quarantine_duration_positive,
        test_results_.back().time_requested);
    absl::Time latest_quarantine_time =
        std::max(latest_symptom_time_, test_results_.back().time_requested) +
        tracing_policy_.quarantine_duration_positive;
    return (timestep.start_time() < latest_quarantine_time) &&
           (timestep.end_time() > earliest_quarantine_time);
  }

  // TODO: Move this to the interface if we need it for actuation.
  // Gets the probability of infection for the associated agent.
  float GetProbabilisticRiskScore() const {
    return 1 - exp(-risk_score_policy_.risk_scale_factor * GetRiskScore());
  }

  float& GetRiskScorePerTimestepById(const size_t id) {
    const size_t idx =
        (head_ + id - head_id_) % risk_score_per_timestep_.size();
    return risk_score_per_timestep_[idx];
  }

  size_t size() const {
    if (tail_ >= head_) return tail_ - head_;
    return risk_score_per_timestep_.size() - head_ + tail_;
  }

  // Appends a risk score value to the historical record.
  absl::Status AppendRiskScoreForExposure(const float risk_score,
                                          const Exposure& exposure) {
    if (risk_score_per_timestep_.empty()) {
      return absl::OutOfRangeError(
          "Expecting historical record of risk scores to be non-empty.");
    }
    auto lb = std::lower_bound(
        timestep_to_id_.begin(), timestep_to_id_.end(), exposure.start_time,
        [](const std::pair<Timestep, size_t>& timestep_id,
           const absl::Time start_time) {
          return timestep_id.first.end_time() < start_time;
        });
    if (lb == timestep_to_id_.end()) {
      std::ostringstream o;
      o << "Exposure " << exposure << " is out of range. Latest timestep: "
        << timestep_to_id_.rbegin()->first;
      return absl::OutOfRangeError(o.str());
    } else if (lb->first.start_time() > exposure.start_time) {
      std::ostringstream o;
      o << "Exposure " << exposure << " is out of range. Earliest timestep: "
        << timestep_to_id_.begin()->first;
      return absl::OutOfRangeError(o.str());
    } else {
      GetRiskScorePerTimestepById(lb->second) += risk_score;
    }
    return absl::OkStatus();
  }

  const TracingPolicy tracing_policy_;
  const RiskScoreModel& risk_score_model_;
  const LearningRiskScorePolicy& risk_score_policy_;
  const LocationTypeFn location_type_;
  absl::Time infection_onset_time_;
  std::vector<TestResult> test_results_;
  absl::Time latest_symptom_time_;
  absl::Time latest_contact_time_;
  std::vector<float> risk_score_per_timestep_;
  size_t head_ = 0;
  size_t tail_ = 0;
  size_t head_id_ = 1;
  // A mapping of observed timesteps to the indices of risk_score_per_timestep_.
  // Used for garbage collection and accounting with (potentially
  // variable-length) timesteps.
  std::map<Timestep, size_t, TimestepComparator> timestep_to_id_;
  absl::optional<Timestep> latest_timestep_;
};

class AppEnabledRiskScore : public RiskScore {
 public:
  explicit AppEnabledRiskScore(const bool is_app_enabled,
                               std::unique_ptr<RiskScore> risk_score)
      : is_app_enabled_(is_app_enabled), risk_score_(std::move(risk_score)) {}

  void AddHealthStateTransistion(HealthTransition transition) override {
    risk_score_->AddHealthStateTransistion(transition);
  }
  void UpdateLatestTimestep(const Timestep& timestep) override {
    risk_score_->UpdateLatestTimestep(timestep);
  }
  void AddExposureNotification(const Exposure& exposure,
                               const ContactReport& notification) override {
    if (is_app_enabled_) {
      risk_score_->AddExposureNotification(exposure, notification);
    }
  }
  VisitAdjustment GetVisitAdjustment(const Timestep& timestep,
                                     const int64 location_uuid) const override {
    return risk_score_->GetVisitAdjustment(timestep, location_uuid);
  }
  TestResult GetTestResult(const Timestep& timestep) const override {
    return risk_score_->GetTestResult(timestep);
  }
  ContactTracingPolicy GetContactTracingPolicy(
      const Timestep& timestep) const override {
    if (is_app_enabled_) {
      return risk_score_->GetContactTracingPolicy(timestep);
    }
    return {.report_recursively = false, .send_report = false};
  }
  absl::Duration ContactRetentionDuration() const override {
    return risk_score_->ContactRetentionDuration();
  }
  float GetRiskScore() const override { return risk_score_->GetRiskScore(); }
  void RequestTest(const absl::Time time) override {
    risk_score_->RequestTest(time);
  }

 private:
  const bool is_app_enabled_;
  std::unique_ptr<RiskScore> risk_score_;
};

class HazardQueryingRiskScore : public RiskScore {
 public:
  explicit HazardQueryingRiskScore(std::unique_ptr<Hazard> hazard,
                                   std::unique_ptr<RiskScore> risk_score)
      : hazard_(std::move(hazard)), risk_score_(std::move(risk_score)) {}

  void AddHealthStateTransistion(HealthTransition transition) override {
    risk_score_->AddHealthStateTransistion(transition);
  }
  void UpdateLatestTimestep(const Timestep& timestep) override {
    risk_score_->UpdateLatestTimestep(timestep);
  }
  void AddExposureNotification(const Exposure& exposure,
                               const ContactReport& notification) override {
    risk_score_->AddExposureNotification(exposure, notification);
  }
  VisitAdjustment GetVisitAdjustment(const Timestep& timestep,
                                     const int64 location_uuid) const override {
    return risk_score_->GetVisitAdjustment(timestep, location_uuid);
  }
  TestResult GetTestResult(const Timestep& timestep) const override {
    const float hazard = hazard_->GetHazard(timestep);
    // Get a test depending on the current hazard. Not realistic
    // but useful for understanding learning dynamics.
    if (absl::GetFlag(FLAGS_request_test_using_hazard) &&
        risk_score_->GetTestResult(timestep).time_requested ==
            absl::InfiniteFuture() &&
        absl::Bernoulli(GetBitGen(), hazard)) {
      risk_score_->RequestTest(timestep.start_time());
    }
    TestResult result = risk_score_->GetTestResult(timestep);
    result.hazard = hazard;
    return result;
  }
  ContactTracingPolicy GetContactTracingPolicy(
      const Timestep& timestep) const override {
    return risk_score_->GetContactTracingPolicy(timestep);
  }
  absl::Duration ContactRetentionDuration() const override {
    return risk_score_->ContactRetentionDuration();
  }
  float GetRiskScore() const override { return risk_score_->GetRiskScore(); }
  void RequestTest(const absl::Time time) override {
    risk_score_->RequestTest(time);
  }

 private:
  std::unique_ptr<Hazard> hazard_;
  std::unique_ptr<RiskScore> risk_score_;
};

absl::Status DurationFromProto(const google::protobuf::Duration& duration_proto,
                               absl::Duration* duration) {
  auto duration_or = DecodeGoogleApiProto(duration_proto);
  if (!duration_or.ok()) {
    return duration_or.status();
  }
  *duration = *duration_or;
  return absl::OkStatus();
}

absl::StatusOr<TracingPolicy> FromProto(
    const TracingPolicyProto& tracing_policy_proto) {
  TracingPolicy tracing_policy;

  PANDEMIC_RETURN_IF_ERROR(
      DurationFromProto(tracing_policy_proto.quarantine_duration_contacts(),
                        &tracing_policy.quarantine_duration_contacts));
  PANDEMIC_RETURN_IF_ERROR(
      DurationFromProto(tracing_policy_proto.quarantine_duration_risk_score(),
                        &tracing_policy.quarantine_duration_risk_score));
  PANDEMIC_RETURN_IF_ERROR(
      DurationFromProto(tracing_policy_proto.quarantine_duration_symptoms(),
                        &tracing_policy.quarantine_duration_symptoms));
  PANDEMIC_RETURN_IF_ERROR(
      DurationFromProto(tracing_policy_proto.quarantine_duration_positive(),
                        &tracing_policy.quarantine_duration_positive));
  if (tracing_policy_proto.quarantine_risk_score_threshold() < 0 ||
      tracing_policy_proto.quarantine_risk_score_threshold() > 1) {
    return absl::InvalidArgumentError(
        "Quarantine risk score threshold not within [0, 1].");
  }
  tracing_policy.quarantine_risk_score_threshold =
      tracing_policy_proto.quarantine_risk_score_threshold();

  tracing_policy.quarantine_on_symptoms = absl::Bernoulli(
      GetBitGen(), tracing_policy_proto.self_quarantine_on_symptoms_fraction());

  PANDEMIC_RETURN_IF_ERROR(
      DurationFromProto(tracing_policy_proto.test_validity_duration(),
                        &tracing_policy.test_validity_duration));
  if (!tracing_policy_proto.has_test_properties()) {
    return absl::InvalidArgumentError("Config is missing test properties.");
  }
  PANDEMIC_RETURN_IF_ERROR(
      DurationFromProto(tracing_policy_proto.test_properties().latency(),
                        &tracing_policy.test_latency));
  if (tracing_policy_proto.test_properties().sensitivity() <= 0 ||
      tracing_policy_proto.test_properties().sensitivity() > 1) {
    return absl::InvalidArgumentError("Test sensitivity not within (0, 1].");
  }
  tracing_policy.test_sensitivity =
      tracing_policy_proto.test_properties().sensitivity();
  if (tracing_policy_proto.test_properties().specificity() <= 0 ||
      tracing_policy_proto.test_properties().specificity() > 1) {
    return absl::InvalidArgumentError("Test specificity not within (0, 1].");
  }
  tracing_policy.test_specificity =
      tracing_policy_proto.test_properties().specificity();

  tracing_policy.test_on_symptoms = tracing_policy_proto.test_on_symptoms();
  if (tracing_policy_proto.test_risk_score_threshold() < 0 ||
      tracing_policy_proto.test_risk_score_threshold() > 1) {
    return absl::InvalidArgumentError(
        "Test risk score on threshold not within [0, 1].");
  }
  tracing_policy.test_risk_score_threshold =
      tracing_policy_proto.test_risk_score_threshold();
  tracing_policy.test_all_per_timestep =
      tracing_policy_proto.test_all_per_timestep();
  tracing_policy.test_on_contact = tracing_policy_proto.test_on_contact();
  PANDEMIC_RETURN_IF_ERROR(
      DurationFromProto(tracing_policy_proto.contact_retention_duration(),
                        &tracing_policy.contact_retention_duration));
  tracing_policy.trace_on_positive = tracing_policy_proto.trace_on_positive();
  if (tracing_policy_proto.traceable_interaction_fraction() < 0 ||
      tracing_policy_proto.traceable_interaction_fraction() > 1) {
    return absl::InvalidArgumentError(
        "Traceable interaction franction not within [0, 1].");
  }
  tracing_policy.traceable_interaction_fraction =
      tracing_policy_proto.traceable_interaction_fraction();
  return tracing_policy;
}

// A static representation of the risk score model loaded at simulation start.
// All usages of this class should be const& to prevent us creating a new copy
// for each agent.
class LearningRiskScoreModel : public RiskScoreModel {
 public:
  LearningRiskScoreModel() {}
  LearningRiskScoreModel(
      const std::vector<BLEBucket>& ble_buckets,
      const std::vector<InfectiousnessBucket>& infectiousness_buckets)
      : ble_buckets_(ble_buckets),
        infectiousness_buckets_(infectiousness_buckets) {}

  float ComputeRiskScore(
      const Exposure& exposure,
      absl::optional<absl::Time> initial_symptom_onset_time) const;

 private:
  absl::StatusOr<int> AttenuationToBinIndex(const int attenuation) const;

  float ComputeDurationRiskScore(const Exposure& exposure) const;
  // Note: This method assumes infectiousness_buckets_ has a particular
  // ordering. Specifically the ordering is asc on days_since_symptom_onset_max.
  float ComputeInfectionRiskScore(
      absl::optional<int64> days_since_symptom_onset) const;

  // Buckets representing threshold and corresponding weight of ble attenuation
  // signals.
  std::vector<BLEBucket> ble_buckets_;
  // Buckets representing days_since_symptom onset and a mapping to a
  // corresponding infectiousness level and model weight.
  std::vector<InfectiousnessBucket> infectiousness_buckets_;
};

class TimeVaryingRiskScoreModel : public RiskScoreModel {
 public:
  explicit TimeVaryingRiskScoreModel(
      std::function<const RiskScoreModel*()> get_model_fn)
      : get_model_fn_(get_model_fn) {}

  float ComputeRiskScore(
      const Exposure& exposure,
      absl::optional<absl::Time> initial_symptom_onset_time) const override {
    return get_model_fn_()->ComputeRiskScore(exposure,
                                             initial_symptom_onset_time);
  }

 private:
  std::function<const RiskScoreModel* const()> get_model_fn_;
};

}  // namespace

float LearningRiskScoreModel::ComputeRiskScore(
    const Exposure& exposure,
    absl::optional<absl::Time> initial_symptom_onset_time) const {
  absl::optional<int64> days_since_symptom_onset;
  if (initial_symptom_onset_time.has_value()) {
    days_since_symptom_onset = ConvertDurationToDiscreteDays(
        exposure.start_time - initial_symptom_onset_time.value());
  }
  float risk_score = 1;

  risk_score *= ComputeDurationRiskScore(exposure);
  risk_score *= ComputeInfectionRiskScore(days_since_symptom_onset);

  return risk_score;
}

float LearningRiskScoreModel::ComputeDurationRiskScore(
    const Exposure& exposure) const {
  std::vector<absl::Duration> ble_bucket_durations;
  ble_bucket_durations.resize(ble_buckets_.size(), absl::ZeroDuration());

  const absl::StatusOr<int> bin_index_or =
      AttenuationToBinIndex(exposure.attenuation);
  if (!bin_index_or.ok()) {
    LOG(ERROR) << "Unable to properly compute duration score: "
               << bin_index_or.status() << ". Falling back to 0.";
    return 0;
  }
  const int bin_index = bin_index_or.value();
  const float duration_score = ble_buckets_[bin_index].weight() *
                               absl::ToInt64Minutes(kProximityTraceInterval);

  VLOG(1) << "duration_score: " << duration_score << " exposure: " << exposure;

  return duration_score;
}

float LearningRiskScoreModel::ComputeInfectionRiskScore(
    absl::optional<int64> days_since_symptom_onset) const {
  for (const InfectiousnessBucket& bucket : infectiousness_buckets_) {
    if (!days_since_symptom_onset.has_value()) {
      if (bucket.level() == InfectiousnessLevel::UNKNOWN) {
        return bucket.weight();
      }
    } else {
      bool inside_bucket_bounds = days_since_symptom_onset.value() >=
                                      bucket.days_since_symptom_onset_min() &&
                                  days_since_symptom_onset.value() <=
                                      bucket.days_since_symptom_onset_max();
      if (inside_bucket_bounds) {
        return bucket.weight();
      }
    }
  }
  // TODO: Handle asymptomatics by using test date (and some bucket
  // assignment).
  VLOG(1) << "No valid infectiousness bucket found for "
          << "days_since_symptom_onset: "
          << (days_since_symptom_onset.has_value()
                  ? days_since_symptom_onset.value()
                  : -1LL)
          << ". Setting infection score to 0.";
  return 0;
}

absl::StatusOr<int> LearningRiskScoreModel::AttenuationToBinIndex(
    const int attenuation) const {
  for (int i = 0; i < ble_buckets_.size(); ++i) {
    if (attenuation <= ble_buckets_[i].max_attenuation()) {
      return i;
    }
  }
  return absl::InvalidArgumentError(
      absl::StrCat("Attenuation value ", attenuation,
                   " larger than: ", ble_buckets_.back().max_attenuation()));
}

absl::StatusOr<const LearningRiskScorePolicy> CreateLearningRiskScorePolicy(
    const LearningRiskScorePolicyProto& proto) {
  if (proto.risk_scale_factor() <= 0) {
    return absl::InvalidArgumentError(absl::StrCat(
        "Invalid value found for risk_scale_factor:", proto.risk_scale_factor(),
        ". Must be a positive, non-zero value."));
  }

  if (proto.exposure_notification_window_days() <= 0) {
    return absl::InvalidArgumentError(absl::StrCat(
        "Invalid value found for exposure_notification_window_days: ",
        proto.exposure_notification_window_days(),
        ". Must be a positive, non-zero value."));
  }

  LearningRiskScorePolicy policy = {
      .exposure_notification_window_days =
          static_cast<int>(proto.exposure_notification_window_days()),
      .risk_scale_factor = proto.risk_scale_factor()};

  return policy;
}

absl::StatusOr<std::unique_ptr<RiskScoreModel>> CreateLearningRiskScoreModel(
    const LearningRiskScoreModelProto& proto) {
  std::vector<BLEBucket> ble_buckets;
  for (const BLEBucket& bucket : proto.ble_buckets()) {
    ble_buckets.push_back(bucket);
  }
  if (ble_buckets.empty()) {
    return absl::InvalidArgumentError("BLEBuckets is empty.");
  }

  // Note: Buckets must be sorted in asc order on threshold value. This is
  // assumed downstream in ComputeDurationRiskScore.
  std::sort(ble_buckets.begin(), ble_buckets.end(),
            [](const BLEBucket& a, const BLEBucket& b) {
              return a.max_attenuation() < b.max_attenuation();
            });

  std::vector<InfectiousnessBucket> infectiousness_buckets;
  bool contains_unknown_level = false;
  for (const InfectiousnessBucket& bucket : proto.infectiousness_buckets()) {
    infectiousness_buckets.push_back(bucket);
    if (bucket.level() == InfectiousnessLevel::NONE) {
      contains_unknown_level = true;
    }
  }

  if (infectiousness_buckets.empty()) {
    return absl::InvalidArgumentError("Infectiousness buckets is empty.");
  }

  if (!contains_unknown_level) {
    return absl::InvalidArgumentError(
        "Infectiousness buckets must contain an entry for NONE infectiousness "
        "level with bounds set to [-inf, inf].");
  }

  // Note: Buckets must be sorted in asc order using max threshold value. This
  // is assumed downstream in ComputeInfectionRiskScore.
  std::sort(infectiousness_buckets.begin(), infectiousness_buckets.end(),
            [](const InfectiousnessBucket& a, const InfectiousnessBucket& b) {
              return a.days_since_symptom_onset_max() <
                     b.days_since_symptom_onset_max();
            });

  return absl::make_unique<LearningRiskScoreModel>(ble_buckets,
                                                   infectiousness_buckets);
}

absl::StatusOr<std::unique_ptr<RiskScore>> CreateLearningRiskScore(
    const TracingPolicyProto& tracing_policy_proto,
    const LearningRiskScorePolicy& risk_score_policy,
    const RiskScoreModel* risk_score_model, LocationTypeFn location_type) {
  // TODO: Consider creating a Builder that stores this as a data
  // member rather than creating it for every RiskScore instance.
  PANDEMIC_ASSIGN_OR_RETURN(const TracingPolicy tracing_policy,
                            FromProto(tracing_policy_proto));
  return absl::make_unique<LearningRiskScore>(tracing_policy, risk_score_model,
                                              risk_score_policy, location_type);
}

std::unique_ptr<RiskScore> CreateAppEnabledRiskScore(
    const bool is_app_enabled, std::unique_ptr<RiskScore> risk_score) {
  return absl::make_unique<AppEnabledRiskScore>(is_app_enabled,
                                                std::move(risk_score));
}

std::unique_ptr<RiskScore> CreateHazardQueryingRiskScore(
    std::unique_ptr<Hazard> hazard, std::unique_ptr<RiskScore> risk_score) {
  return absl::make_unique<HazardQueryingRiskScore>(std::move(hazard),
                                                    std::move(risk_score));
}

}  // namespace abesim
