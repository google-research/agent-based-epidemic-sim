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

#include "absl/memory/memory.h"
#include "absl/time/time.h"
#include "agent_based_epidemic_sim/applications/contact_tracing/config.pb.h"
#include "agent_based_epidemic_sim/applications/contact_tracing/risk_score.h"
#include "agent_based_epidemic_sim/applications/home_work/location_type.h"
#include "agent_based_epidemic_sim/core/integral_types.h"
#include "agent_based_epidemic_sim/core/pandemic.pb.h"
#include "agent_based_epidemic_sim/port/time_proto_util.h"

namespace abesim {

namespace {

struct TracingRiskScoreConfig {
  absl::Duration test_validity_duration;
  absl::Duration contact_retention_duration;
  absl::Duration quarantine_duration;
  absl::Duration test_latency;
  float positive_threshold;
};

// A policy that implements testing, tracing, and isolation guidelines.
class TracingRiskScore : public RiskScore {
 public:
  TracingRiskScore(LocationTypeFn location_type,
                   const TracingRiskScoreConfig& tracing_policy)
      : tracing_policy_(tracing_policy),
        location_type_(std::move(location_type)),
        latest_health_state_(HealthState::SUSCEPTIBLE),
        test_result_({.time_requested = absl::InfiniteFuture(),
                      .time_received = absl::InfiniteFuture(),
                      .needs_retry = false,
                      .probability = 0}),
        latest_contact_time_(absl::InfinitePast()) {}

  void AddHealthStateTransistion(HealthTransition transition) override {
    latest_health_state_ = transition.health_state;
  }
  void AddExposures(absl::Span<const Exposure* const> exposures) override {}
  void AddExposureNotification(const Contact& contact,
                               const TestResult& result) override {
    latest_contact_time_ =
        std::max(latest_contact_time_,
                 contact.exposure.start_time + contact.exposure.duration);
  }
  void AddTestResult(const TestResult& result) override {
    test_result_ = result;
  }

  VisitAdjustment GetVisitAdjustment(const Timestep& timestep,
                                     const int64 location_uuid) const override {
    const bool skip_visit =
        location_type_(location_uuid) != LocationType::kHome &&
        (ShouldQuarantineFromContacts(timestep) ||
         ShouldQuarantineFromSymptoms());
    return {
        .frequency_adjustment = skip_visit ? 0.0f : 1.0f,
        .duration_adjustment = 1.0f,
    };
  }

  TestPolicy GetTestPolicy(const Timestep& timestep) const override {
    // TODO: handle symptom-based test requests.
    TestPolicy policy;
    if (test_result_.needs_retry) {
      return {.should_test = true,
              .time_requested = test_result_.time_requested,
              .latency = tracing_policy_.test_latency};
    }
    if (NeedsNewTestFromContacts(timestep)) {
      LOG(ERROR) << "st " << latest_contact_time_;
      return {.should_test = true,
              .time_requested = latest_contact_time_,
              .latency = tracing_policy_.test_latency};
    }
    return {.should_test = false};
  }

  ContactTracingPolicy GetContactTracingPolicy() const override {
    return {.report_recursively = false, .send_positive_test = true};
  }

  absl::Duration ContactRetentionDuration() const override {
    return tracing_policy_.contact_retention_duration;
  }

 private:
  bool HasRetainedPositiveContact(const Timestep& timestep) const {
    return latest_contact_time_ >=
           timestep.start_time() - ContactRetentionDuration();
  }
  bool NeedsNewTestFromContacts(const Timestep& timestep) const {
    if (!HasRetainedPositiveContact(timestep)) {
      // No retained positive contact.
      return false;
    }
    if (test_result_.time_received == absl::InfiniteFuture()) {
      // Has not yet requested a test.
      return true;
    }
    if (test_result_.time_received >
        latest_contact_time_ - tracing_policy_.test_validity_duration) {
      // Test result still valid.
      return false;
    }
    // Test result (if negative) is stale : retest.
    return test_result_.probability < tracing_policy_.positive_threshold;
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
  bool ShouldQuarantineFromSymptoms() const {
    // TODO: It seems that this should consider recovered patients
    // and also the time at which the latest health state became active.
    return latest_health_state_ != HealthState::SUSCEPTIBLE;
  }

  const TracingRiskScoreConfig tracing_policy_;
  const LocationTypeFn location_type_;
  HealthState::State latest_health_state_;
  TestResult test_result_;
  absl::Time latest_contact_time_;
};

}  // namespace

StatusOr<std::unique_ptr<RiskScore>> CreateTracingRiskScore(
    const TracingPolicyProto& proto, LocationTypeFn location_type) {
  TracingRiskScoreConfig config;
  auto test_validity_duration_or =
      DecodeGoogleApiProto(proto.test_validity_duration());
  if (!test_validity_duration_or.ok()) {
    return test_validity_duration_or.status();
  }
  config.test_validity_duration = *test_validity_duration_or;
  auto contact_retention_duration_or =
      DecodeGoogleApiProto(proto.contact_retention_duration());
  if (!contact_retention_duration_or.ok()) {
    return contact_retention_duration_or.status();
  }
  config.contact_retention_duration = *contact_retention_duration_or;
  auto quarantine_duration_or =
      DecodeGoogleApiProto(proto.quarantine_duration());
  if (!quarantine_duration_or.ok()) {
    return quarantine_duration_or.status();
  }
  config.quarantine_duration = *quarantine_duration_or;
  auto test_latency_or = DecodeGoogleApiProto(proto.test_latency());
  if (!test_latency_or.ok()) {
    return test_latency_or.status();
  }
  config.test_latency = *test_latency_or;
  config.positive_threshold = proto.positive_threshold();
  return absl::make_unique<TracingRiskScore>(location_type, config);
}

}  // namespace abesim
