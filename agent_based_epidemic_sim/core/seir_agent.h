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

#ifndef AGENT_BASED_EPIDEMIC_SIM_CORE_SEIR_AGENT_H_
#define AGENT_BASED_EPIDEMIC_SIM_CORE_SEIR_AGENT_H_

#include <algorithm>

#include "absl/container/flat_hash_set.h"
#include "absl/time/time.h"
#include "absl/types/span.h"
#include "agent_based_epidemic_sim/core/agent.h"
#include "agent_based_epidemic_sim/core/broker.h"
#include "agent_based_epidemic_sim/core/event.h"
#include "agent_based_epidemic_sim/core/integral_types.h"
#include "agent_based_epidemic_sim/core/risk_score.h"
#include "agent_based_epidemic_sim/core/transition_model.h"
#include "agent_based_epidemic_sim/core/transmission_model.h"
#include "agent_based_epidemic_sim/core/visit.h"
#include "agent_based_epidemic_sim/core/visit_generator.h"

namespace abesim {

constexpr std::array<HealthState::State, 2> kNotInfectedHealthStates = {
    HealthState::SUSCEPTIBLE, HealthState::REMOVED};

inline bool IsInfectedState(const HealthState::State& subject_health_state) {
  return std::find(kNotInfectedHealthStates.begin(),
                   kNotInfectedHealthStates.end(),
                   subject_health_state) == kNotInfectedHealthStates.end();
}

// An agent that implements a stochastic SEIR model.
class SEIRAgent : public Agent {
 public:
  // Convenience factory method to construct a SUSCEPTIBLE agent.
  static std::unique_ptr<SEIRAgent> CreateSusceptible(
      const int64 uuid, TransmissionModel* transmission_model,
      std::unique_ptr<TransitionModel> transition_model,
      std::unique_ptr<VisitGenerator> visit_generator,
      std::unique_ptr<RiskScore> risk_score);

  // Constructs an agent with a specified health state transition.
  static std::unique_ptr<SEIRAgent> Create(
      const int64 uuid, const HealthTransition& health_transition,
      TransmissionModel* transmission_model,
      std::unique_ptr<TransitionModel> transition_model,
      std::unique_ptr<VisitGenerator> visit_generator,
      std::unique_ptr<RiskScore> risk_score);

  SEIRAgent(const SEIRAgent&) = delete;
  SEIRAgent& operator=(const SEIRAgent&) = delete;

  int64 uuid() const override { return uuid_; }

  // Computes the set of visits that an agent will make in a given timestep and
  // calculates health states for those visits.
  void ComputeVisits(const Timestep& timestep,
                     Broker<Visit>* visit_broker) const override;

  // Receives contact reports from agents contacted in previous timesteps and
  // send new contact reports to prior contacts.  Also performs clinical tests
  // to be performed during the current timestep.
  void UpdateContactReports(const Timestep& timestep,
                            absl::Span<const ContactReport> contact_reports,
                            Broker<ContactReport>* broker) override;

  // Updates health state from infections.
  void ProcessInfectionOutcomes(
      const Timestep& timestep,
      absl::Span<const InfectionOutcome> infection_outcomes) override;

  HealthState::State CurrentHealthState() const override {
    return health_transitions_.back().health_state;
  }

  TestResult CurrentTestResult() const override { return test_result_; }

  absl::Span<const HealthTransition> HealthTransitions() const override {
    return absl::Span<const HealthTransition>(health_transitions_.data(),
                                              health_transitions_.size());
  }

  // For use in testing.
  HealthTransition NextHealthTransition() const {
    return next_health_transition_;
  }

 private:
  SEIRAgent(const int64 uuid, const HealthTransition& initial_health_transition,
            TransmissionModel* transmission_model,
            std::unique_ptr<TransitionModel> transition_model,
            std::unique_ptr<VisitGenerator> visit_generator,
            std::unique_ptr<RiskScore> risk_score)
      : uuid_(uuid),
        test_result_({.time_requested = absl::InfiniteFuture(),
                      .time_received = absl::InfiniteFuture(),
                      .needs_retry = false,
                      .probability = 0}),
        transmission_model_(transmission_model),
        transition_model_(std::move(transition_model)),
        visit_generator_(std::move(visit_generator)),
        risk_score_(std::move(risk_score)) {
    next_health_transition_ = initial_health_transition;
    health_transitions_.push_back({.time = absl::InfinitePast(),
                                   .health_state = HealthState::SUSCEPTIBLE});
    risk_score_->AddTestResult(test_result_);
    risk_score_->AddHealthStateTransistion(health_transitions_.back());
  }

  // Computes infectivity of agent at a given time.
  float CurrentInfectivity(const absl::Time& current_time) const;

  // Advances the health state transitions.
  void MaybeUpdateHealthTransitions(const Timestep& timestep);
  // Splits visits on HealthTransition boundaries so that a unique HealthState
  // can be assigned to each visit.
  void SplitAndAssignHealthStates(std::vector<Visit>* visits) const;

  // May carry out a test depending on the given test policy.
  // TODO: Move this logic to public policy implementation.
  void MaybeTest(const RiskScore::TestPolicy& test_policy,
                 TestResult* test_result);

  void SendContactReports(
      const RiskScore::ContactTracingPolicy& contact_tracing_policy,
      absl::Span<const ContactReport> received_reports,
      Broker<ContactReport>* broker) const;

  absl::Duration DurationSinceFirstInfection(
      const absl::Time& current_time) const;

  const int64 uuid_;
  // The health state changes this agent has observed. Ordered in chronological
  // order. Note that the next pending state transition is stored in
  // next_health_transition for ease of notation.
  // TODO: Implement state persistence and clean up.
  std::vector<HealthTransition> health_transitions_;
  HealthTransition next_health_transition_;
  absl::optional<absl::Time> initial_infection_time_;

  TestResult test_result_;

  using ContactList = std::list<Contact>;
  struct ContactHasher {
    using is_transparent = void;
    size_t operator()(ContactList::iterator contact) const {
      return contact->other_uuid;
    }
    size_t operator()(int64 other_uuid) const { return other_uuid; }
  };
  struct ContactEq {
    using is_transparent = void;
    bool operator()(ContactList::iterator left,
                    ContactList::iterator right) const {
      return left->other_uuid == right->other_uuid;
    }
    bool operator()(int64 left, ContactList::iterator right) const {
      return left == right->other_uuid;
    }
    bool operator()(ContactList::iterator left, int64 right) const {
      return left->other_uuid == right;
    }
    bool operator()(int64 left, int64 right) const { return left == right; }
  };
  ContactList contacts_;
  absl::flat_hash_set<ContactList::iterator, ContactHasher, ContactEq>
      contact_set_;

  // Unowned (shared between agents at risk for the given disease).
  TransmissionModel* const transmission_model_;
  // TODO: It may be possible to share the transition_model. The
  // visit_generator will likely be initialized uniquely for the agent, but may
  // be shared among "equivalence" classes of agents.
  std::unique_ptr<TransitionModel> transition_model_;
  std::unique_ptr<VisitGenerator> visit_generator_;
  std::unique_ptr<RiskScore> risk_score_;
};

}  // namespace abesim

#endif  // AGENT_BASED_EPIDEMIC_SIM_CORE_SEIR_AGENT_H_
