/*
 * Copyright 2020 Google LLC
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#ifndef THIRD_PARTY_AGENT_BASED_EPIDEMIC_SIM_CORE_PUBLIC_POLICY_H_
#define THIRD_PARTY_AGENT_BASED_EPIDEMIC_SIM_CORE_PUBLIC_POLICY_H_

#include <memory>

#include "absl/types/span.h"
#include "agent_based_epidemic_sim/core/event.h"
#include "agent_based_epidemic_sim/core/integral_types.h"
#include "agent_based_epidemic_sim/core/pandemic.pb.h"
#include "agent_based_epidemic_sim/core/timestep.h"

namespace pandemic {

// PublicPolicy represents government enacted policy choices as they apply to a
// particular agent.
class PublicPolicy {
 public:
  struct VisitAdjustment {
    float frequency_adjustment;
    float duration_adjustment;

    friend bool operator==(const VisitAdjustment& a, const VisitAdjustment& b) {
      return (a.frequency_adjustment == b.frequency_adjustment &&
              a.duration_adjustment == b.duration_adjustment);
    }

    friend bool operator!=(const VisitAdjustment& a, const VisitAdjustment& b) {
      return !(a == b);
    }

    friend std::ostream& operator<<(std::ostream& strm,
                                    const VisitAdjustment& visit_adjustment) {
      return strm << "{" << visit_adjustment.frequency_adjustment << ", "
                  << visit_adjustment.duration_adjustment << "}";
    }
  };

  // Encapsulates whether and how to request a test. Contains the following:
  // - whether a test should be conducted
  // - the time at which the test is requested
  // - the duration for receiving a result from the test.
  struct TestPolicy {
    bool should_test;
    absl::Time time_requested;
    absl::Duration latency;

    friend bool operator==(const TestPolicy& a, const TestPolicy& b) {
      return (a.should_test == b.should_test &&
              a.time_requested == b.time_requested && a.latency == b.latency);
    }

    friend bool operator!=(const TestPolicy& a, const TestPolicy& b) {
      return !(a == b);
    }

    friend std::ostream& operator<<(std::ostream& strm,
                                    const TestPolicy& test_policy) {
      return strm << "{" << test_policy.should_test << ","
                  << test_policy.time_requested << "," << test_policy.latency
                  << "}";
    }
  };

  // Encapsulates which contact reports to forward.
  struct ContactTracingPolicy {
    bool report_recursively;
    bool send_positive_test;

    friend bool operator==(const ContactTracingPolicy& a,
                           const ContactTracingPolicy& b) {
      return (a.report_recursively == b.report_recursively &&
              a.send_positive_test == b.send_positive_test);
    }

    friend bool operator!=(const ContactTracingPolicy& a,
                           const ContactTracingPolicy& b) {
      return !(a == b);
    }

    friend std::ostream& operator<<(
        std::ostream& strm,
        const ContactTracingPolicy& contact_tracing_policy) {
      return strm << "{" << contact_tracing_policy.report_recursively << ", "
                  << contact_tracing_policy.send_positive_test << "}";
    }
  };

  // Get the adjustment a particular agent should make to it's visits to the
  // given location assuming the given current_health_state.
  // Note that different agents can have different policies.  For exmample
  // an essential employee may see no adjustment, whereas a non-essential
  // employee may be banned from the same location.
  virtual VisitAdjustment GetVisitAdjustment(
      const Timestep& timestep, HealthState::State health_state,
      const ContactSummary& contact_summary, int64 location_uuid) const = 0;

  // TODO: Widen interface to accept self-reported symptoms.
  // Gets guidance on whether the agent should be tested.
  virtual TestPolicy GetTestPolicy(
      const ContactSummary& contact_summary,
      const TestResult& previous_test_result) const = 0;

  // Gets the policy to be used when sending contact reports.
  virtual ContactTracingPolicy GetContactTracingPolicy(
      absl::Span<const ContactReport> received_contact_reports,
      const TestResult& test_result) const = 0;

  // Gets the duration for which to retain contacts.
  virtual absl::Duration ContactRetentionDuration() const = 0;

  virtual ~PublicPolicy() = default;
};

// Samples PublicPolicy instances.
class PolicyGenerator {
 public:
  // Get a policy for the next worker.
  virtual const PublicPolicy* NextPolicy() = 0;
  virtual ~PolicyGenerator() = default;
};

// Returns a policy that never adjusts visits.
std::unique_ptr<PublicPolicy> NewNoOpPolicy();

}  // namespace pandemic

#endif  // THIRD_PARTY_AGENT_BASED_EPIDEMIC_SIM_CORE_PUBLIC_POLICY_H_
