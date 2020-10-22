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

struct LearningRiskScoreConfig {
  absl::Duration test_validity_duration;
  absl::Duration contact_retention_duration;
  absl::Duration quarantine_duration;
  absl::Duration test_latency;
};

// A policy that implements testing, tracing, and isolation guidelines.
class LearningRiskScore : public RiskScore {
 public:
  LearningRiskScore(LocationTypeFn location_type,
                    const LearningRiskScoreConfig& tracing_policy,
                    const LearningRiskScoreModel& risk_score_model)
      : tracing_policy_(tracing_policy),
        location_type_(std::move(location_type)),
        risk_score_model_(risk_score_model),
        infection_onset_time_(absl::InfiniteFuture()),
        latest_contact_time_(absl::InfinitePast()) {}

  void AddHealthStateTransistion(HealthTransition transition) override {
    // QUESTION: Should this exclude EXPOSED?
    if (transition.health_state != HealthState::SUSCEPTIBLE) {
      infection_onset_time_ = std::min(infection_onset_time_, transition.time);
    }
  }
  void AddExposures(absl::Span<const Exposure* const> exposures) override {}
  void AddExposureNotification(const Exposure& exposure,
                               const ContactReport& notification) override {
    // We don't take action on negative tests.
    if (notification.test_result.outcome != TestOutcome::POSITIVE) return;

    const float risk_score = risk_score_model_.ComputeRiskScore(
        exposure, notification.initial_symptom_onset_time);
    VLOG(1) << "Risk score is " << risk_score << " for exposure: " << exposure;
    // TODO: implement circular buffer maintaining a per day history of
    // risk_score sum.
    current_risk_score_sum_ += risk_score;

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

  const LearningRiskScoreConfig tracing_policy_;
  const LocationTypeFn location_type_;
  const LearningRiskScoreModel& risk_score_model_;
  absl::Time infection_onset_time_;
  std::vector<TestResult> test_results_;
  absl::Time latest_contact_time_;
  float current_risk_score_sum_;
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

  if (proto.exposure_notification_window_days() <= 0) {
    return absl::InvalidArgumentError(
        "exposure_notification_window_days must be a positive integer.");
  }
  const LearningRiskScoreModel model = LearningRiskScoreModel(
      proto.risk_scale_factor(), ble_buckets, infectiousness_buckets,
      proto.exposure_notification_window_days());
  return model;
}

absl::StatusOr<std::unique_ptr<RiskScore>> CreateLearningRiskScore(
    const TracingPolicyProto& tracing_policy_proto,
    const LearningRiskScoreModel& risk_score_model,
    LocationTypeFn location_type) {
  LearningRiskScoreConfig config;
  auto test_validity_duration_or =
      DecodeGoogleApiProto(tracing_policy_proto.test_validity_duration());
  if (!test_validity_duration_or.ok()) {
    return test_validity_duration_or.status();
  }
  config.test_validity_duration = *test_validity_duration_or;
  auto contact_retention_duration_or =
      DecodeGoogleApiProto(tracing_policy_proto.contact_retention_duration());
  if (!contact_retention_duration_or.ok()) {
    return contact_retention_duration_or.status();
  }
  config.contact_retention_duration = *contact_retention_duration_or;
  auto quarantine_duration_or =
      DecodeGoogleApiProto(tracing_policy_proto.quarantine_duration());
  if (!quarantine_duration_or.ok()) {
    return quarantine_duration_or.status();
  }
  config.quarantine_duration = *quarantine_duration_or;
  auto test_latency_or =
      DecodeGoogleApiProto(tracing_policy_proto.test_latency());
  if (!test_latency_or.ok()) {
    return test_latency_or.status();
  }
  config.test_latency = *test_latency_or;
  LearningRiskScore risk_score(location_type, config, risk_score_model);

  return absl::make_unique<LearningRiskScore>(location_type, config,
                                              risk_score_model);
}

}  // namespace abesim
