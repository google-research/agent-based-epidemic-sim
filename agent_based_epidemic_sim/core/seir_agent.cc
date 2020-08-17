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

#include <cmath>
#include <iterator>
#include <memory>

#include "absl/time/time.h"
#include "agent_based_epidemic_sim/core/constants.h"
#include "agent_based_epidemic_sim/port/logging.h"

namespace abesim {
namespace {

bool ContactFromBefore(const Contact& contact, const absl::Time time) {
  return contact.exposure.start_time + contact.exposure.duration < time;
}

// TODO: Move to a more appropriate location when this gets more
// sophisticated like taking into account covariates.
float SymptomFactor(const HealthState::State health_state) {
  // Symptoms are not infectious.
  if (health_state == HealthState::SUSCEPTIBLE ||
      health_state == HealthState::RECOVERED ||
      health_state == HealthState::REMOVED) {
    return 0.0f;
  }

  // Symptoms are mildly infectious.
  if (health_state == HealthState::ASYMPTOMATIC) {
    return 0.33f;
  }

  // Symptoms are moderately infectious.
  if (health_state == HealthState::PRE_SYMPTOMATIC_MILD ||
      health_state == HealthState::SYMPTOMATIC_MILD) {
    return 0.72f;
  }

  // Symptoms are severely infectious.
  return 1.0f;
}
}  // namespace

/* static */
std::unique_ptr<SEIRAgent> SEIRAgent::CreateSusceptible(
    const int64 uuid, TransmissionModel* transmission_model,
    std::unique_ptr<TransitionModel> transition_model,
    std::unique_ptr<VisitGenerator> visit_generator,
    std::unique_ptr<RiskScore> risk_score) {
  return SEIRAgent::Create(uuid,
                           {.time = absl::InfiniteFuture(),
                            .health_state = HealthState::SUSCEPTIBLE},
                           transmission_model, std::move(transition_model),
                           std::move(visit_generator), std::move(risk_score));
}

/* static */
std::unique_ptr<SEIRAgent> SEIRAgent::Create(
    const int64 uuid, const HealthTransition& health_transition,
    TransmissionModel* transmission_model,
    std::unique_ptr<TransitionModel> transition_model,
    std::unique_ptr<VisitGenerator> visit_generator,
    std::unique_ptr<RiskScore> risk_score) {
  return absl::WrapUnique(new SEIRAgent(
      uuid, health_transition, transmission_model, std::move(transition_model),
      std::move(visit_generator), std::move(risk_score)));
}

void SEIRAgent::SplitAndAssignHealthStates(std::vector<Visit>* visits) const {
  auto interval = health_transitions_.rbegin();
  for (int i = visits->size() - 1; i >= 0;) {
    Visit& visit = (*visits)[i];
    visit.health_state = interval->health_state;
    visit.infectivity = CurrentInfectivity(visit.start_time);
    visit.symptom_factor = SymptomFactor(interval->health_state);
    visit.agent_uuid = uuid_;
    if (visit.start_time >= interval->time) {
      --i;
      continue;
    }
    if (visit.end_time > interval->time) {
      Visit split_visit = visit;
      visit.end_time = interval->time;
      // No visit should ever come before the first health transition.
      visit.symptom_factor = SymptomFactor((interval + 1)->health_state);
      split_visit.start_time = interval->time;
      split_visit.infectivity = CurrentInfectivity(split_visit.start_time);
      split_visit.symptom_factor = SymptomFactor(interval->health_state);
      visits->push_back(split_visit);
    }
    ++interval;
  }
}

void SEIRAgent::MaybeUpdateHealthTransitions(const Timestep& timestep) {
  while (next_health_transition_.time < timestep.end_time()) {
    const absl::Time original_transition_time = next_health_transition_.time;
    if (IsInfectedState(next_health_transition_.health_state) &&
        !initial_infection_time_.has_value()) {
      initial_infection_time_ = original_transition_time;
    }
    health_transitions_.push_back(next_health_transition_);
    risk_score_->AddHealthStateTransistion(next_health_transition_);
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
  visit_generator_->GenerateVisits(timestep, *risk_score_, &visits);
  SplitAndAssignHealthStates(&visits);
  visit_broker->Send(visits);
}

void SEIRAgent::UpdateContactReports(
    const Timestep& timestep, absl::Span<const ContactReport> contact_reports,
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
    risk_score_->AddExposureNotification(**contact, contact_report.test_result);
  }
  SendContactReports(timestep, contact_reports, broker);
}

void SEIRAgent::SendContactReports(
    const Timestep& timestep, absl::Span<const ContactReport> received_reports,
    Broker<ContactReport>* broker) {
  const RiskScore::ContactTracingPolicy& contact_tracing_policy =
      risk_score_->GetContactTracingPolicy(timestep);
  if (contact_tracing_policy.report_recursively) {
    LOG(DFATAL) << "Recursive contact tracing not yet supported.";
  }
  if (!contact_tracing_policy.send_report) return;

  const TestResult test_result = risk_score_->GetTestResult(timestep);
  if (test_result != last_test_result_sent_) {
    // We want to avoid re-sending ContactReports for contacts we've already
    // sent a ContactReport for.  We keep track of
    // last_contact_report_considered_ which tells us the last contact we sent
    // last_test_result_sent_ for.  If we get a new test result, though, we
    // need to send that even for contacts we sent the previous result for.
    // For that reason we reset last_contact_report_consiered_ to an invalid
    // value here so we send the new test result to all our contacts.
    last_contact_report_considered_ = contacts_.end();
    last_test_result_sent_ = test_result;
  }

  std::vector<ContactReport> contact_reports;
  for (auto contact = contacts_.rbegin(); contact != contacts_.rend();
       ++contact) {
    if (last_contact_report_considered_ != contacts_.end() &&
        last_contact_report_considered_ == std::prev(contact.base())) {
      break;
    }
    contact_reports.push_back({.from_agent_uuid = uuid(),
                               .to_agent_uuid = contact->other_uuid,
                               .test_result = test_result});
  }
  last_contact_report_considered_ = std::prev(contacts_.end());

  broker->Send(contact_reports);
}

absl::Duration SEIRAgent::DurationSinceFirstInfection(
    const absl::Time& current_time) const {
  if (initial_infection_time_.has_value()) {
    return current_time - initial_infection_time_.value();
  }
  return absl::InfiniteDuration();
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
        if (iter == last_contact_report_considered_) {
          last_contact_report_considered_ =
              iter == contacts_.begin() ? contacts_.end() : std::prev(iter);
        }
        contact_set_.erase(previous_contact);
        contacts_.erase(iter);
      }
      contact_set_.insert(contacts_.insert(contacts_.end(), contact));
      exposures.push_back(&infection_outcome.exposure);
    }
  }
  risk_score_->AddExposures(exposures);
  if (next_health_transition_.health_state == HealthState::SUSCEPTIBLE &&
      !exposures.empty()) {
    const HealthTransition health_transition =
        transmission_model_->GetInfectionOutcome(exposures);
    if (health_transition.health_state == HealthState::EXPOSED) {
      next_health_transition_ = health_transition;
    }
  }
  const absl::Time earliest_retained_contact_time =
      timestep.start_time() - risk_score_->ContactRetentionDuration();
  while (
      !contacts_.empty() &&
      ContactFromBefore(*contacts_.begin(), earliest_retained_contact_time)) {
    if (contacts_.begin() == last_contact_report_considered_) {
      last_contact_report_considered_ = contacts_.end();
    }
    contact_set_.erase(contacts_.begin());
    contacts_.erase(contacts_.begin());
  }
  MaybeUpdateHealthTransitions(timestep);
}

float SEIRAgent::CurrentInfectivity(const absl::Time& current_time) const {
  if (!IsInfectedState(CurrentHealthState()) ||
      !initial_infection_time_.has_value() ||
      current_time < initial_infection_time_) {
    return 0;
  }

  const absl::Duration duration_since_infection =
      DurationSinceFirstInfection(current_time);
  const int discrete_days_since_infection =
      (int)std::round(absl::ToDoubleHours(duration_since_infection) / 24.0f);

  if (discrete_days_since_infection > 14) return 0;

  return kInfectivityArray[discrete_days_since_infection];
}

}  // namespace abesim
