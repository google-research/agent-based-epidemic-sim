#include "agent_based_epidemic_sim/core/hazard_transmission_model.h"

#include <array>
#include <iterator>
#include <numeric>
#include <vector>

#include "absl/random/distributions.h"
#include "absl/time/time.h"
#include "absl/types/span.h"
#include "agent_based_epidemic_sim/core/constants.h"
#include "agent_based_epidemic_sim/core/integral_types.h"
#include "agent_based_epidemic_sim/port/logging.h"

namespace abesim {

// TODO: Fix variable naming in this implementation. For now it is
// easiest to simply mirror the python implementation until it is stable.
float HazardTransmissionModel::ComputeDose(const float distance,
                                           absl::Duration duration,
                                           const Exposure* exposure) {
  const float t = absl::ToDoubleMinutes(duration);
  const float fd = risk_at_distance_function_(distance);

  // TODO: Thread these factors through on the Exposure in the
  // Location phase (e.g. Location::ProcessVisits).
  const float finf = exposure->infectivity;
  const float fsym = exposure->symptom_factor;
  const float floc = exposure->location_transmissibility;
  const float fage = exposure->susceptibility;

  const float h = t * fd * finf * fsym * floc * fage;
  return h;
}

HealthTransition HazardTransmissionModel::GetInfectionOutcome(
    absl::Span<const Exposure* const> exposures) {
  absl::Time latest_exposure_time = absl::InfinitePast();
  float sum_dose = 0;
  for (const Exposure* exposure : exposures) {
    latest_exposure_time = std::max(latest_exposure_time,
                                    exposure->start_time + exposure->duration);
    for (const float& proximity : exposure->proximity_trace.values) {
      sum_dose += ComputeDose(proximity, kProximityTraceInterval, exposure);
    }
  }

  const float prob_infection = 1 - std::exp(-lambda_ * sum_dose);
  HealthTransition health_transition;
  health_transition.time = latest_exposure_time;
  health_transition.health_state = absl::Bernoulli(gen_, prob_infection)
                                       ? HealthState::EXPOSED
                                       : HealthState::SUSCEPTIBLE;
  return health_transition;
}
}  // namespace abesim
