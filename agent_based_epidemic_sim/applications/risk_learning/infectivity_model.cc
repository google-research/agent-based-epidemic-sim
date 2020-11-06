#include "agent_based_epidemic_sim/applications/risk_learning/infectivity_model.h"

#include <cmath>

#include "absl/time/time.h"
#include "agent_based_epidemic_sim/core/constants.h"

namespace abesim {

RiskLearningInfectivityModel::RiskLearningInfectivityModel(
    const GlobalProfile& profile)
    : asymptomatic_infectious_factor_(profile.asymptomatic_infectious_factor()),
      mild_infectious_factor_(profile.mild_infectious_factor()) {}

float RiskLearningInfectivityModel::SymptomFactor(
    HealthState::State health_state) const {
  // TODO: Add reference to source of HealthState -> factor mapping.
  switch (health_state) {
    // Symptoms are not infectious.
    case HealthState::SUSCEPTIBLE:
    case HealthState::RECOVERED:
    case HealthState::REMOVED:
    case HealthState::EXPOSED:
    case HealthState::SYMPTOMATIC_HOSPITALIZED:
    case HealthState::SYMPTOMATIC_HOSPITALIZED_RECOVERING:
      return 0.0f;
    // Asymptomatic but infectious.
    case HealthState::ASYMPTOMATIC:
      return asymptomatic_infectious_factor_;
    // Mildly symptomatic.
    case HealthState::PRE_SYMPTOMATIC_MILD:
    case HealthState::SYMPTOMATIC_MILD:
      return mild_infectious_factor_;
    // Other symptoms are severely infectious.
    default:
      return 1.0f;
  }
}

float RiskLearningInfectivityModel::Infectivity(
    const absl::Duration duration_since_infection) const {
  const int discrete_days_since_infection =
      duration_since_infection >= absl::ZeroDuration()
          ? (duration_since_infection + absl::Hours(12)) / absl::Hours(24)
          : -1;

  return 0 <= discrete_days_since_infection &&
                 discrete_days_since_infection <= 14
             ? kInfectivityArray[discrete_days_since_infection]
             : 0.0f;
}

}  // namespace abesim
