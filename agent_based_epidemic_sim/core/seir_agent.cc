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
#include "agent_based_epidemic_sim/core/event.h"
#include "agent_based_epidemic_sim/core/visit.h"
#include "agent_based_epidemic_sim/port/logging.h"
#include "agent_based_epidemic_sim/util/time_utils.h"

namespace abesim {
namespace {

// TODO: Move to a more appropriate location when this gets more
// sophisticated like taking into account covariates.
float SymptomFactor(const HealthState::State health_state) {
  // Symptoms are not infectious.
  if (health_state == HealthState::SUSCEPTIBLE ||
      health_state == HealthState::RECOVERED ||
      health_state == HealthState::REMOVED ||
      health_state == HealthState::EXPOSED) {
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
    const VisitGenerator& visit_generator,
    std::unique_ptr<RiskScore> risk_score,
    VisitLocationDynamics visit_dynamics) {
  return SEIRAgent::Create(uuid,
                           {.time = absl::InfiniteFuture(),
                            .health_state = HealthState::SUSCEPTIBLE},
                           transmission_model, std::move(transition_model),
                           visit_generator, std::move(risk_score),
                           std::move(visit_dynamics));
}

/* static */
std::unique_ptr<SEIRAgent> SEIRAgent::Create(
    const int64 uuid, const HealthTransition& health_transition,
    TransmissionModel* transmission_model,
    std::unique_ptr<TransitionModel> transition_model,
    const VisitGenerator& visit_generator,
    std::unique_ptr<RiskScore> risk_score,
    VisitLocationDynamics visit_dynamics) {
  return absl::WrapUnique(new SEIRAgent(
      uuid, health_transition, transmission_model, std::move(transition_model),
      visit_generator, std::move(risk_score), std::move(visit_dynamics)));
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

void SEIRAgent::SeedInfection(const absl::Time time) {
  SetNextHealthTransition({
      .time = time,
      .health_state = HealthState::EXPOSED,
  });
  UpdateHealthTransition(Timestep(time, absl::Seconds(1LL)));
}

void SEIRAgent::UpdateHealthTransition(const Timestep& timestep) {
  const absl::Time original_transition_time = next_health_transition_.time;
  if (IsInfectedState(next_health_transition_.health_state) &&
      !initial_infection_time_.has_value()) {
    initial_infection_time_ = original_transition_time;
  }
  if (IsSymptomaticState(next_health_transition_.health_state) &&
      !initial_symptom_onset_time_.has_value()) {
    initial_symptom_onset_time_ = original_transition_time;
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

void SEIRAgent::MaybeUpdateHealthTransitions(const Timestep& timestep) {
  while (next_health_transition_.time < timestep.end_time()) {
    UpdateHealthTransition(timestep);
  }
}

void SEIRAgent::AssignVisitDynamics(std::vector<Visit>* visits) const {
  for (Visit& visit : *visits) {
    visit.location_dynamics = visit_dynamics_;
  }
}

void SEIRAgent::ComputeVisits(const Timestep& timestep,
                              Broker<Visit>* visit_broker) const {
  thread_local std::vector<Visit> visits;
  visits.clear();
  visit_generator_.GenerateVisits(timestep, *risk_score_, &visits);
  SplitAndAssignHealthStates(&visits);
  AssignVisitDynamics(&visits);
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
    exposures_.ProcessNotification(
        contact_report, [this, &contact_report](const Exposure& exposure) {
          risk_score_->AddExposureNotification(exposure, contact_report);
        });
  }
  SendContactReports(timestep, broker);
}

void SEIRAgent::SendContactReports(const Timestep& timestep,
                                   Broker<ContactReport>* broker) {
  const RiskScore::ContactTracingPolicy& contact_tracing_policy =
      risk_score_->GetContactTracingPolicy(timestep);
  if (contact_tracing_policy.report_recursively) {
    LOG(DFATAL) << "Recursive contact tracing not yet supported.";
  }
  if (!contact_tracing_policy.send_report) return;

  const TestResult test_result = risk_score_->GetTestResult(timestep);
  if (test_result != last_test_result_sent_) {
    contact_report_send_cutoff_ = absl::InfinitePast();
    last_test_result_sent_ = test_result;
  }

  std::vector<ContactReport> contact_reports;
  exposures_.PerAgent(
      contact_report_send_cutoff_,
      [this, &test_result, &contact_reports](const int64 uuid) {
        contact_reports.push_back({
            .from_agent_uuid = this->uuid(),
            .to_agent_uuid = uuid,
            .test_result = test_result,
            .initial_symptom_onset_time = initial_symptom_onset_time_,
        });
      });
  contact_report_send_cutoff_ = timestep.start_time();
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

  const absl::Time earliest_retained_contact_time =
      timestep.start_time() - risk_score_->ContactRetentionDuration();
  exposures_.GarbageCollect(earliest_retained_contact_time);
  exposures_.AddExposures(infection_outcomes);

  std::vector<const Exposure*> exposures;
  exposures.reserve(infection_outcomes.size());
  for (const InfectionOutcome& infection_outcome : infection_outcomes) {
    exposures.push_back(&infection_outcome.exposure);
  }
  risk_score_->AddExposures(timestep, exposures);

  if (next_health_transition_.health_state == HealthState::SUSCEPTIBLE &&
      !exposures.empty()) {
    const HealthTransition health_transition =
        transmission_model_->GetInfectionOutcome(exposures);
    if (health_transition.health_state == HealthState::EXPOSED) {
      next_health_transition_ = health_transition;
    }
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
      ConvertDurationToDiscreteDays(duration_since_infection);

  if (discrete_days_since_infection > 14) return 0;

  return kInfectivityArray[discrete_days_since_infection];
}

}  // namespace abesim
