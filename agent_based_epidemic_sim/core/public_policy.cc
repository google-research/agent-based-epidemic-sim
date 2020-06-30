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

#include "agent_based_epidemic_sim/core/public_policy.h"

#include "absl/memory/memory.h"

namespace abesim {

namespace {

class NullPublicPolicy : public PublicPolicy {
 public:
  // Get the adjustment a particular agent should make to it's visits to the
  // given location assuming the given current_health_state.
  // Note that different agents can have different policies.  For exmample
  // an essential employee may see no adjustment, whereas a non-essential
  // employee may be banned from the same location.
  VisitAdjustment GetVisitAdjustment(const Timestep& timestep,
                                     HealthState::State health_state,
                                     const ContactSummary& contact_summary,
                                     int64 location_uuid) const override {
    return {.frequency_adjustment = 1.0, .duration_adjustment = 1.0};
  }

  TestPolicy GetTestPolicy(
      const ContactSummary& contact_summary,
      const TestResult& previous_test_result) const override {
    return {.should_test = false,
            .time_requested = absl::InfiniteFuture(),
            .latency = absl::InfiniteDuration()};
  }

  ContactTracingPolicy GetContactTracingPolicy(
      absl::Span<const ContactReport> received_contact_reports,
      const TestResult& test_result) const override {
    return {.report_recursively = false, .send_positive_test = false};
  }

  absl::Duration ContactRetentionDuration() const override {
    return absl::ZeroDuration();
  }
};

}  // namespace

std::unique_ptr<PublicPolicy> NewNoOpPolicy() {
  return absl::make_unique<NullPublicPolicy>();
}

}  // namespace abesim
