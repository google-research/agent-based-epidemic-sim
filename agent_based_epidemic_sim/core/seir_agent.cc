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

#include "agent_based_epidemic_sim/core/seir_agent.h"

#include "absl/time/time.h"
#include "agent_based_epidemic_sim/port/logging.h"

namespace pandemic {
namespace {
bool ContactFromBefore(const Contact& contact, const absl::Time time) {
  return contact.exposure.start_time + contact.exposure.duration < time;
}
}  // namespace

/* static */
std::unique_ptr<SEIRAgent> SEIRAgent::CreateSusceptible(
    const int64 uuid, TransmissionModel* transmission_model,
    std::unique_ptr<TransitionModel> transition_model,
    std::unique_ptr<VisitGenerator> visit_generator,
    const PublicPolicy* public_policy) {
  return SEIRAgent::Create(uuid,
                           {.time = absl::InfiniteFuture(),
                            .health_state = HealthState::SUSCEPTIBLE},
                           transmission_model, std::move(transition_model),
                           std::move(visit_generator), public_policy);
}

/* static */
std::unique_ptr<SEIRAgent> SEIRAgent::Create(
    const int64 uuid, const HealthTransition& health_transition,
    TransmissionModel* transmission_model,
    std::unique_ptr<TransitionModel> transition_model,
    std::unique_ptr<VisitGenerator> visit_generator,
    const PublicPolicy* public_policy) {
  return absl::WrapUnique(new SEIRAgent(
      uuid, health_transition, transmission_model, std::move(transition_model),
      std::move(visit_generator), public_policy));
}

void SEIRAgent::SplitAndAssignHealthStates(std::vector<Visit>* visits) const {
  auto interval = health_transitions_.rbegin();
  for (int i = visits->size() - 1; i >= 0;) {
    Visit& visit = (*visits)[i];
    visit.health_state = interval->health_state;
    visit.agent_uuid = uuid_;
    if (visit.start_time >= interval->time) {
      --i;
    } else {
      if (visit.end_time > interval->time) {
        Visit split_visit = visit;
        visit.end_time = interval->time;
        split_visit.start_time = interval->time;
        visits->push_back(split_visit);
      }
      ++interval;
    }
  }
}

void SEIRAgent::MaybeUpdateHealthTransitions(const Timestep& timestep) {
  while (next_health_transition_.time < timestep.end_time()) {
    const absl::Time original_transition_time = next_health_transition_.time;
    health_transitions_.push_back(next_health_transition_);
    next_health_transition_ =
        transition_model_->GetNextHealthTransition(next_health_transition_);
    absl::Duration health_state_duration =
        next_health_transition_.time - original_transition_time;
    if (health_state_duration < timestep.duration()) {
      // TODO: Clean up enforcement of minimums/maximums on dwell times,
      // particularly for long-running (recurrent) states like SUSCEPTIBLE.
      next_health_transition_.time =
          original_transition_time + timestep.duration();
    }
  }
}

void SEIRAgent::ComputeVisits(const Timestep& timestep,
                              Broker<Visit>* visit_broker) const {
  thread_local std::vector<Visit> visits;
  visits.clear();
  visit_generator_->GenerateVisits(timestep, public_policy_,
                                   CurrentHealthState(), GetContactSummary(),
                                   &visits);
  SplitAndAssignHealthStates(&visits);
  visit_broker->Send(visits);
}

void SEIRAgent::UpdateContactReports(
    absl::Span<const ContactReport> contact_reports,
    Broker<ContactReport>* broker) {
  auto matches_uuid_fn =
      [this](const absl::Span<const ContactReport> contact_reports) {
        return std::all_of(contact_reports.begin(), contact_reports.end(),
                           [this](const ContactReport& contact_report) {
                             if (contact_report.to_agent_uuid != uuid()) {
                               LOG(WARNING)
                                   << "Incorrect ContactReport to_agent_uuid: "
                                   << contact_report.to_agent_uuid;
                               return false;
                             }
                             return true;
                           });
      };
  DCHECK(matches_uuid_fn(contact_reports))
      << "Found incorrect ContactReport uuid.";
  for (const ContactReport& contact_report : contact_reports) {
    auto contact = contact_set_.find(contact_report.from_agent_uuid);
    if (contact == contact_set_.end()) continue;
    contact_summary_.latest_contact_time = std::max(
        (*contact)->exposure.start_time + (*contact)->exposure.duration,
        contact_summary_.latest_contact_time);
  }
  MaybeTest(public_policy_->GetTestPolicy(contact_summary_, test_result_),
            &test_result_);

  // TODO: Cache already-sent internal report to avoid resending
  // identical report.
  SendContactReports(
      public_policy_->GetContactTracingPolicy(contact_reports, test_result_),
      contact_reports, broker);
}

void SEIRAgent::MaybeTest(const PublicPolicy::TestPolicy& test_policy,
                          TestResult* test_result) {
  if (!test_policy.should_test) {
    return;
  }
  test_result->needs_retry = false;
  test_result->time_requested = test_policy.time_requested;
  if (next_health_transition_.time < test_result->time_requested) {
    // Too early to test, must retry at a later time.
    test_result->needs_retry = true;
    return;
  }
  test_result->time_received =
      test_result->time_requested + test_policy.latency;
  // TODO: Sample from calibrated Bernoulli for test
  // sensitivity/specificity.
  for (auto transition = health_transitions_.rbegin();
       transition != health_transitions_.rend(); ++transition) {
    test_result->probability =
        transition->health_state == HealthState::SUSCEPTIBLE ? 0 : 1;
    if (transition->time < test_policy.time_requested) {
      break;
    }
  }
}

void SEIRAgent::SendContactReports(
    const PublicPolicy::ContactTracingPolicy& contact_tracing_policy,
    absl::Span<const ContactReport> received_reports,
    Broker<ContactReport>* broker) const {
  if (contact_tracing_policy.report_recursively) {
    LOG(DFATAL) << "Recursive contact tracing not yet supported.";
  }
  if (contact_tracing_policy.send_positive_test &&
      test_result_.probability == 1) {
    std::vector<ContactReport> contact_reports;
    contact_reports.reserve(contacts_.size());
    for (const Contact& contact : contacts_) {
      contact_reports.push_back({.from_agent_uuid = uuid(),
                                 .to_agent_uuid = contact.other_uuid,
                                 .test_result = test_result_});
    }
    broker->Send(contact_reports);
  }
}

void SEIRAgent::ProcessInfectionOutcomes(
    const Timestep& timestep,
    const absl::Span<const InfectionOutcome> infection_outcomes) {
  auto matches_uuid_fn =
      [this](const absl::Span<const InfectionOutcome> infection_outcomes) {
        return std::all_of(infection_outcomes.begin(), infection_outcomes.end(),
                           [this](const InfectionOutcome& infection_outcome) {
                             if (infection_outcome.agent_uuid != uuid()) {
                               LOG(WARNING)
                                   << "Incorrect InfectionOutcome agent_uuid: "
                                   << infection_outcome.agent_uuid;
                               return false;
                             }
                             return true;
                           });
      };
  DCHECK(matches_uuid_fn(infection_outcomes))
      << "Found incorrect InfectionOutcome uuid.";
  std::vector<const Exposure*> exposures;
  exposures.reserve(infection_outcomes.size());
  for (const InfectionOutcome& infection_outcome : infection_outcomes) {
    // TODO: Record background exposures.
    if (infection_outcome.exposure_type == InfectionOutcomeProto::CONTACT) {
      const Contact contact{.other_uuid = infection_outcome.source_uuid,
                            .exposure = infection_outcome.exposure};
      const auto previous_contact =
          contact_set_.find(infection_outcome.source_uuid);
      if (previous_contact != contact_set_.end()) {
        std::list<Contact>::iterator iter = *previous_contact;
        contact_set_.erase(previous_contact);
        contacts_.erase(iter);
      }
      contact_set_.insert(contacts_.insert(contacts_.end(), contact));
      exposures.push_back(&infection_outcome.exposure);
    }
  }
  if (next_health_transition_.health_state == HealthState::SUSCEPTIBLE &&
      !exposures.empty()) {
    const HealthTransition health_transition =
        transmission_model_->GetInfectionOutcome(exposures);
    if (health_transition.health_state == HealthState::EXPOSED) {
      next_health_transition_ = health_transition;
    }
  }
  const absl::Time earliest_retained_contact_time =
      timestep.start_time() - public_policy_->ContactRetentionDuration();
  while (
      !contacts_.empty() &&
      ContactFromBefore(*contacts_.begin(), earliest_retained_contact_time)) {
    contact_set_.erase(contacts_.begin());
    contacts_.erase(contacts_.begin());
  }
  contact_summary_.retention_horizon = earliest_retained_contact_time;
  MaybeUpdateHealthTransitions(timestep);
}

}  // namespace pandemic
