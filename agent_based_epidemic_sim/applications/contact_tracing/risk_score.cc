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
#include "absl/status/statusor.h"
#include "absl/time/time.h"
#include "agent_based_epidemic_sim/applications/contact_tracing/config.pb.h"
#include "agent_based_epidemic_sim/applications/contact_tracing/risk_score.h"
#include "agent_based_epidemic_sim/core/event.h"
#include "agent_based_epidemic_sim/core/integral_types.h"
#include "agent_based_epidemic_sim/core/location_type.h"
#include "agent_based_epidemic_sim/core/pandemic.pb.h"
#include "agent_based_epidemic_sim/port/time_proto_util.h"

namespace abesim {

namespace {

struct TracingRiskScoreConfig {
  absl::Duration test_validity_duration;
  absl::Duration contact_retention_duration;
  absl::Duration quarantine_duration;
  absl::Duration test_latency;
};

// A policy that implements testing, tracing, and isolation guidelines.
class TracingRiskScore : public RiskScore {
 public:
  TracingRiskScore(LocationTypeFn location_type,
                   const TracingRiskScoreConfig& tracing_policy)
      : tracing_policy_(tracing_policy),
        location_type_(std::move(location_type)),
        infection_onset_time_(absl::InfiniteFuture()),
        latest_contact_time_(absl::InfinitePast()) {}

  void AddHealthStateTransistion(HealthTransition transition) override {
    // TODO: Should this exclude EXPOSED?
    if (transition.health_state != HealthState::SUSCEPTIBLE) {
      infection_onset_time_ = std::min(infection_onset_time_, transition.time);
    }
  }
  void AddExposures(const Timestep& timestep,
                    absl::Span<const Exposure* const> exposures) override {}
  void AddExposureNotification(const Exposure& exposure,
                               const ContactReport& notification) override {
    // We don't take action on negative tests.
    if (notification.test_result.outcome != TestOutcome::POSITIVE) return;

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
      if (result->time_received < timestep.end_time()) return *result;
    }
    return {.time_requested = absl::InfiniteFuture(),
            .time_received = absl::InfiniteFuture(),
            .outcome = TestOutcome::NEGATIVE};
  }

  ContactTracingPolicy GetContactTracingPolicy(
      const Timestep& timestep) const override {
    const TestResult result = GetTestResult(timestep);
    const bool should_report =
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

  const TracingRiskScoreConfig tracing_policy_;
  const LocationTypeFn location_type_;
  absl::Time infection_onset_time_;
  std::vector<TestResult> test_results_;
  absl::Time latest_contact_time_;
};

}  // namespace

absl::StatusOr<std::unique_ptr<RiskScore>> CreateTracingRiskScore(
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
  return absl::make_unique<TracingRiskScore>(location_type, config);
}

}  // namespace abesim
