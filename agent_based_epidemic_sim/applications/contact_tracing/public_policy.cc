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

#include "agent_based_epidemic_sim/applications/contact_tracing/public_policy.h"

#include <algorithm>

#include "absl/memory/memory.h"
#include "absl/time/time.h"
#include "agent_based_epidemic_sim/applications/contact_tracing/config.pb.h"
#include "agent_based_epidemic_sim/applications/home_work/location_type.h"
#include "agent_based_epidemic_sim/core/integral_types.h"
#include "agent_based_epidemic_sim/core/public_policy.h"
#include "agent_based_epidemic_sim/port/time_proto_util.h"

namespace pandemic {

namespace {

bool HasRetainedPositiveContact(const ContactSummary& contact_summary) {
  return contact_summary.latest_contact_time >=
         contact_summary.retention_horizon;
}

struct TracingPolicyConfig {
  absl::Duration test_validity_duration;
  absl::Duration contact_retention_duration;
  absl::Duration quarantine_duration;
  absl::Duration test_latency;
  float positive_threshold;
};

// A policy that implements testing, tracing, and isolation guidelines.
class TracingPolicy : public PublicPolicy {
 public:
  TracingPolicy(LocationTypeFn location_type,
                const TracingPolicyConfig& tracing_policy)
      : tracing_policy_(tracing_policy),
        location_type_(std::move(location_type)) {}

  VisitAdjustment GetVisitAdjustment(const Timestep& timestep,
                                     HealthState::State health_state,
                                     const ContactSummary& contact_summary,
                                     const int64 location_uuid) const override {
    const bool skip_visit =
        location_type_(location_uuid) != LocationType::kHome &&
        (ShouldQuarantineFromContacts(contact_summary, timestep) ||
         ShouldQuarantineFromSymptoms(health_state));
    return {
        .frequency_adjustment = skip_visit ? 0.0f : 1.0f,
        .duration_adjustment = 1.0f,
    };
  }

  TestPolicy GetTestPolicy(const ContactSummary& contact_summary,
                           const TestResult& test_result) const override {
    // TODO: handle symptom-based test requests.
    TestPolicy policy;
    if (test_result.needs_retry) {
      return {.should_test = true,
              .time_requested = test_result.time_requested,
              .latency = tracing_policy_.test_latency};
    }
    if (NeedsNewTestFromContacts(contact_summary, test_result)) {
      return {.should_test = true,
              .time_requested = contact_summary.latest_contact_time,
              .latency = tracing_policy_.test_latency};
    }
    return {.should_test = false};
  }

  ContactTracingPolicy GetContactTracingPolicy(
      absl::Span<const ContactReport> received_contact_reports,
      const TestResult& test_result) const override {
    return {.report_recursively = false, .send_positive_test = true};
  }

  absl::Duration ContactRetentionDuration() const override {
    return tracing_policy_.contact_retention_duration;
  }

 private:
  bool NeedsNewTestFromContacts(const ContactSummary& contact_summary,
                                const TestResult& test_result) const {
    if (!HasRetainedPositiveContact(contact_summary)) {
      // No retained positive contact.
      return false;
    }
    if (test_result.time_received == absl::InfiniteFuture()) {
      // Has not yet requested a test.
      return true;
    }
    if (test_result.time_received >
        contact_summary.latest_contact_time -
            tracing_policy_.test_validity_duration) {
      // Test result still valid.
      return false;
    }
    // Test result (if negative) is stale : retest.
    return test_result.probability < tracing_policy_.positive_threshold;
  }
  bool ShouldQuarantineFromContacts(const ContactSummary& contact_summary,
                                    const Timestep& timestep) const {
    absl::Time earliest_quarantine_time = std::min(
        contact_summary.retention_horizon, contact_summary.latest_contact_time);
    absl::Time latest_quarantine_time = contact_summary.latest_contact_time +
                                        tracing_policy_.quarantine_duration;
    return (timestep.start_time() < latest_quarantine_time &&
            timestep.end_time() > earliest_quarantine_time);
  }
  bool ShouldQuarantineFromSymptoms(
      const HealthState::State health_state) const {
    return health_state != HealthState::SUSCEPTIBLE;
  }

  TracingPolicyConfig tracing_policy_;
  const LocationTypeFn location_type_;
};

}  // namespace

StatusOr<std::unique_ptr<PublicPolicy>> CreateTracingPolicy(
    const TracingPolicyProto& proto, LocationTypeFn location_type) {
  TracingPolicyConfig config;
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
  return absl::make_unique<TracingPolicy>(location_type, config);
}

}  // namespace pandemic
