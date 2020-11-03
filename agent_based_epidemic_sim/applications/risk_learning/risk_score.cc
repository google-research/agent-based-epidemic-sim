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

#include "absl/memory/memory.h"
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
#include "agent_based_epidemic_sim/port/time_proto_util.h"
#include "agent_based_epidemic_sim/util/time_utils.h"

namespace abesim {

namespace {

struct TracingPolicy {
  absl::Duration test_validity_duration;
  absl::Duration contact_retention_duration;
  absl::Duration quarantine_duration;
  absl::Duration test_latency;
};

// A policy that implements testing, tracing, and isolation guidelines.
class LearningRiskScore : public RiskScore {
 public:
  LearningRiskScore(LocationTypeFn location_type,
                    const TracingPolicy& tracing_policy,
                    const LearningRiskScoreModel& risk_score_model,
                    const LearningRiskScorePolicy& risk_score_policy)
      : tracing_policy_(tracing_policy),
        location_type_(std::move(location_type)),
        risk_score_model_(risk_score_model),
        risk_score_policy_(risk_score_policy),
        infection_onset_time_(absl::InfiniteFuture()),
        latest_contact_time_(absl::InfinitePast()) {}

  void AddHealthStateTransistion(HealthTransition transition) override {
    // QUESTION: Should this exclude EXPOSED?
    if (transition.health_state != HealthState::SUSCEPTIBLE) {
      infection_onset_time_ = std::min(infection_onset_time_, transition.time);
    }
  }
  void AddExposures(const Timestep& timestep,
                    absl::Span<const Exposure* const> exposures) override {
    // Assume this is called each timestep for each agent.
    latest_timestep_ = timestep;
    risk_score_per_timestep_.push_back(0);
  }
  void AddExposureNotification(const Exposure& exposure,
                               const ContactReport& notification) override {
    // Actuate based on app user flag.
    // We don't take action on negative tests.
    if (notification.test_result.outcome != TestOutcome::POSITIVE) return;
    ComputeAndAddExposureRiskScore(exposure,
                                   notification.initial_symptom_onset_time);

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

    // TODO: Introduce some noise into the test result.
    test_results_.push_back({
        .time_requested = notification.test_result.time_received,
        .time_received = request_time + tracing_policy_.test_latency,
        .outcome = request_time >= infection_onset_time_
                       ? TestOutcome::POSITIVE
                       : TestOutcome::NEGATIVE,
    });
  }

  VisitAdjustment GetVisitAdjustment(const Timestep& timestep,
                                     const int64 location_uuid) const override {
    const bool skip_visit =
        location_type_(location_uuid) != LocationReference::HOUSEHOLD &&
        (ShouldQuarantineFromContacts(timestep));
    return {
        .frequency_adjustment = skip_visit ? 0.0f : 1.0f,
        .duration_adjustment = 1.0f,
    };
  }
  TestResult GetTestResult(const Timestep& timestep) const override {
    for (auto result = test_results_.rbegin(); result != test_results_.rend();
         ++result) {
      if (result->time_requested < timestep.end_time()) return *result;
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
        result.outcome == TestOutcome::POSITIVE &&
        result.time_received <= timestep.end_time() &&
        result.time_requested + tracing_policy_.contact_retention_duration >=
            timestep.start_time();

    return {.report_recursively = false, .send_report = should_report};
  }

  absl::Duration ContactRetentionDuration() const override {
    return tracing_policy_.contact_retention_duration;
  }

 private:
  bool HasActiveTest(absl::Time request_time) const {
    return !test_results_.empty() &&
           (test_results_.back().outcome == TestOutcome::POSITIVE ||
            test_results_.back().time_requested +
                    tracing_policy_.test_validity_duration >
                request_time);
  }

  bool ShouldQuarantineFromContacts(const Timestep& timestep) const {
    absl::Time earliest_quarantine_time =
        std::min(timestep.start_time() - ContactRetentionDuration(),
                 latest_contact_time_);
    absl::Time latest_quarantine_time =
        latest_contact_time_ + tracing_policy_.quarantine_duration;
    return (timestep.start_time() < latest_quarantine_time &&
            timestep.end_time() > earliest_quarantine_time);
  }

  // Gets the current risk score of the associated agent.
  float GetCurrentRiskScore() const {
    float total_risk = 0;
    int counter = 0;
    for (auto result = risk_score_per_timestep_.rbegin();
         result != risk_score_per_timestep_.rend() &&
         counter < risk_score_policy_.exposure_notification_window_days_;
         ++result, ++counter) {
      total_risk += *result;
    }
    return total_risk;
  }

  // Gets the probability of infection for the associated agent.
  float GetCurrentProbabilisticRiskScore() const {
    return 1 -
           exp(-risk_score_policy_.risk_scale_factor_ * GetCurrentRiskScore());
  }

  // Computes the risk score for a given exposure.
  void ComputeAndAddExposureRiskScore(
      const Exposure& exposure,
      const absl::optional<absl::Time> initial_symptom_onset_time) {
    const float risk_score = risk_score_model_.ComputeRiskScore(
        exposure, initial_symptom_onset_time);
    VLOG(1) << "Risk score is (" << risk_score
            << ") for exposure: " << exposure;

    if (latest_timestep_.has_value()) {
      if (exposure.start_time >= latest_timestep_.value().end_time() ||
          exposure.start_time < latest_timestep_.value().start_time()) {
        LOG(ERROR) << "Failed to compute risk score. Exposure processed out of "
                      "order. Should be within timestep: "
                   << latest_timestep_.value() << ". Received: " << exposure;
        return;
      }
    } else {
      LOG(WARNING) << "latest_timestep_ is not filled. "
                      "risk_score->AddExposures should be called each timestep "
                      "before risk_score->AddExposureNotification.";
    }

    if (risk_score_per_timestep_.empty()) {
      LOG(ERROR)
          << "Failed to compute risk score: risk_score_per_timestep_ is "
             "empty. Ensure risk_score->AddExposures is called each "
             "timestep before the first risk_score->AddExposureNotification.";
      return;
    }
    risk_score_per_timestep_.back() += risk_score;
  }

  const TracingPolicy tracing_policy_;
  const LocationTypeFn location_type_;
  const LearningRiskScoreModel& risk_score_model_;
  const LearningRiskScorePolicy& risk_score_policy_;
  absl::Time infection_onset_time_;
  std::vector<TestResult> test_results_;
  absl::Time latest_contact_time_;
  std::vector<float> risk_score_per_timestep_;
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
  void AddExposures(const Timestep& timestep,
                    absl::Span<const Exposure* const> exposures) override {
    if (is_app_enabled_) {
      risk_score_->AddExposures(timestep, exposures);
    }
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

 private:
  const bool is_app_enabled_;
  std::unique_ptr<RiskScore> risk_score_;
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

  LOG(WARNING) << "No valid infectiousness bucket found for "
                  "days_since_symptom_onset. Setting infection score to 0.";
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
  LearningRiskScorePolicy policy;
  if (proto.risk_scale_factor() <= 0) {
    return absl::InvalidArgumentError(absl::StrCat(
        "Invalid value found for risk_scale_factor:", proto.risk_scale_factor(),
        ". Must be a positive, non-zero value."));
  }
  policy.risk_scale_factor_ = proto.risk_scale_factor();

  if (proto.exposure_notification_window_days() <= 0) {
    return absl::InvalidArgumentError(absl::StrCat(
        "Invalid value found for exposure_notification_window_days: ",
        proto.exposure_notification_window_days(),
        ". Must be a positive, non-zero value."));
  }
  policy.exposure_notification_window_days_ =
      proto.exposure_notification_window_days();
  return policy;
}

absl::StatusOr<const LearningRiskScoreModel> CreateLearningRiskScoreModel(
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

  const LearningRiskScoreModel model =
      LearningRiskScoreModel(ble_buckets, infectiousness_buckets);
  return model;
}

absl::StatusOr<std::unique_ptr<RiskScore>> CreateLearningRiskScore(
    const TracingPolicyProto& tracing_policy_proto,
    const LearningRiskScoreModel& risk_score_model,
    const LearningRiskScorePolicy& risk_score_policy,
    LocationTypeFn location_type) {
  TracingPolicy tracing_policy;
  auto test_validity_duration_or =
      DecodeGoogleApiProto(tracing_policy_proto.test_validity_duration());
  if (!test_validity_duration_or.ok()) {
    return test_validity_duration_or.status();
  }
  tracing_policy.test_validity_duration = *test_validity_duration_or;
  auto contact_retention_duration_or =
      DecodeGoogleApiProto(tracing_policy_proto.contact_retention_duration());
  if (!contact_retention_duration_or.ok()) {
    return contact_retention_duration_or.status();
  }
  tracing_policy.contact_retention_duration = *contact_retention_duration_or;
  auto quarantine_duration_or =
      DecodeGoogleApiProto(tracing_policy_proto.quarantine_duration());
  if (!quarantine_duration_or.ok()) {
    return quarantine_duration_or.status();
  }
  tracing_policy.quarantine_duration = *quarantine_duration_or;
  auto test_latency_or =
      DecodeGoogleApiProto(tracing_policy_proto.test_latency());
  if (!test_latency_or.ok()) {
    return test_latency_or.status();
  }
  tracing_policy.test_latency = *test_latency_or;

  return absl::make_unique<LearningRiskScore>(
      location_type, tracing_policy, risk_score_model, risk_score_policy);
}

std::unique_ptr<RiskScore> CreateAppEnabledRiskScore(
    const bool is_app_enabled, std::unique_ptr<RiskScore> risk_score) {
  return absl::make_unique<AppEnabledRiskScore>(is_app_enabled,
                                                std::move(risk_score));
}

}  // namespace abesim
