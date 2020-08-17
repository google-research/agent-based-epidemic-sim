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

#ifndef AGENT_BASED_EPIDEMIC_SIM_CORE_AGENT_H_
#define AGENT_BASED_EPIDEMIC_SIM_CORE_AGENT_H_

#include "absl/time/time.h"
#include "absl/types/span.h"
#include "agent_based_epidemic_sim/core/broker.h"
#include "agent_based_epidemic_sim/core/event.h"
#include "agent_based_epidemic_sim/core/integral_types.h"
#include "agent_based_epidemic_sim/core/pandemic.pb.h"
#include "agent_based_epidemic_sim/core/timestep.h"
#include "agent_based_epidemic_sim/core/visit.h"

namespace abesim {

// A simulated agent. Represents an individual that travels to locations and
// has a health state.
// Possibly, ComputeVisits should be separated from ProcessInfectionOutcomes
// via the introduction of separate AgentProcessor interfaces.
// It is expected that for each simulation epoch timestep, the following
// sequence of calls happens:
// ProcessInfectionOutcomes is called to update states (once).
// UpdateContactReports is called zero or more times. If more than once,
// calls should be separated by global barriers.
// ComputeVisits is called to generate the visits the agent will make
// conditioned on its state.
// Example use cases:
// 1) Deliver ContactReports on the next set of calls to Agent.
//    In this mode, one would call in sequence Barrier,
//    ProcessInfectionOutcomes, UpdateContactReports, ComputeVisits, and
//    Barrier. Consuming Agents would not be able to incorporate any new
//    ContactReports until the next set of calls.
// 2) Deliver ContactReports instantaneously.
//    In this mode, one would call in sequence Barrier,
//    ProcessInfectionOutcomes, UpdateContactReports, Barrier,
//    UpdateContactReports, ComputeVisits, Barrier.
//    In the first call to UpdateContactReports, ContactReports would be sent,
//    while in the second, no ContactReports would be received.
// 3) Recursive contact tracing.
//    This mode would proceed similarly to (2), except with potentially a
//    second intermediate barrier to support instantaneous receipt of
//    ContactReports.
class Agent {
 public:
  virtual int64 uuid() const = 0;

  // Computes the set of visits that an agent will make in a given timestep.
  virtual void ComputeVisits(const Timestep& timestep,
                             Broker<Visit>* visit_broker) const = 0;
  // Updates health states from a batch of received InfectionOutcomes resulting
  // from the visits an agent has taken prior to timestep.start_time, and
  // advances the ealth state model over the given timestep.
  virtual void ProcessInfectionOutcomes(
      const Timestep& timestep,
      absl::Span<const InfectionOutcome> infection_outcomes) = 0;

  // Receive contact reports from agents contacted in previous timesteps and
  // send new contact reports to prior contacts.  Also perform clinical tests
  // to be performed during the current timestep.
  // Can be called multiple times for a single Timestep (typically separated by
  // a global barrier).
  virtual void UpdateContactReports(
      const Timestep& timestep, absl::Span<const ContactReport> symptom_reports,
      Broker<ContactReport>* contact_broker) = 0;

  virtual HealthState::State CurrentHealthState() const = 0;

  virtual TestResult CurrentTestResult(const Timestep& timestep) const = 0;

  virtual absl::Span<const HealthTransition> HealthTransitions() const = 0;

  virtual ~Agent() = default;
};

}  // namespace abesim

#endif  // AGENT_BASED_EPIDEMIC_SIM_CORE_AGENT_H_
