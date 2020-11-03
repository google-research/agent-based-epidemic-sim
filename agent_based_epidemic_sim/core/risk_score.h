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

#ifndef AGENT_BASED_EPIDEMIC_SIM_CORE_RISK_SCORE_H_
#define AGENT_BASED_EPIDEMIC_SIM_CORE_RISK_SCORE_H_

#include <memory>

#include "absl/time/time.h"
#include "absl/types/span.h"
#include "agent_based_epidemic_sim/core/event.h"
#include "agent_based_epidemic_sim/core/timestep.h"

namespace abesim {

class RiskScore {
 public:
  // Informs the RiskScore of a HealthTransition.
  virtual void AddHealthStateTransistion(HealthTransition transition) = 0;
  // Informs the RiskScore of new exposures.
  virtual void AddExposures(const Timestep& timestep,
                            absl::Span<const Exposure* const> exposures) = 0;
  // Informs the RiskScore of received exposure notifications.
  virtual void AddExposureNotification(const Exposure& exposure,
                                       const ContactReport& notification) = 0;

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

  // Get the adjustment a particular agent should make to it's visits to the
  // given location.
  // Note that different agents can have different policies.  For exmample
  // an essential employee may see no adjustment, whereas a non-essential
  // employee may be banned from the same location.
  virtual VisitAdjustment GetVisitAdjustment(const Timestep& timestep,
                                             int64 location_uuid) const = 0;

  // Get the test result that is relevant for the given timestep.
  virtual TestResult GetTestResult(const Timestep& timestep) const = 0;

  // Encapsulates which contact reports to forward.
  struct ContactTracingPolicy {
    bool report_recursively;
    bool send_report;

    friend bool operator==(const ContactTracingPolicy& a,
                           const ContactTracingPolicy& b) {
      return (a.report_recursively == b.report_recursively &&
              a.send_report == b.send_report);
    }

    friend bool operator!=(const ContactTracingPolicy& a,
                           const ContactTracingPolicy& b) {
      return !(a == b);
    }

    friend std::ostream& operator<<(
        std::ostream& strm,
        const ContactTracingPolicy& contact_tracing_policy) {
      return strm << "{" << contact_tracing_policy.report_recursively << ", "
                  << contact_tracing_policy.send_report << "}";
    }
  };
  // Gets the policy to be used when sending contact reports.
  virtual ContactTracingPolicy GetContactTracingPolicy(
      const Timestep& timestep) const = 0;

  // Gets the duration for which to retain contacts.
  virtual absl::Duration ContactRetentionDuration() const = 0;

  virtual ~RiskScore() = default;
};

// Samples RiskScore instances.
class RiskScoreGenerator {
 public:
  // Get a policy for the next worker.
  virtual std::unique_ptr<RiskScore> NextRiskScore() = 0;
  virtual ~RiskScoreGenerator() = default;
};

std::unique_ptr<RiskScore> NewNullRiskScore();

}  // namespace abesim

#endif  // AGENT_BASED_EPIDEMIC_SIM_CORE_RISK_SCORE_H_
