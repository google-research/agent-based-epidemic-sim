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

#include "agent_based_epidemic_sim/core/aggregated_transmission_model.h"

#include "absl/random/distributions.h"

namespace abesim {
namespace {
// TODO: Move into the visit message about the visiting agent.
constexpr float kSusceptibility = 1;
constexpr double kEpsilon = 1e-8;

// For now this regularizes proximity [0, 100] to distance [0, 10]. According to
// https://journals.plos.org/plosone/article?id=10.1371/journal.pone.0100915
// bluetooth signal strength has a range of up to 10m.
float ProximityToDistance(float proximity) { return proximity / 10; }

// As noted in (broken link).
float DistanceToInfectionProbability(float distance) {
  float denominator = 1 + std::exp(-1.5 * distance + 6.6);
  return 1 - (1 / denominator);
}

float ProbabilityExposureInfects(const Exposure& exposure,
                                 const float transmissibility) {
  float distance = ProximityToDistance(exposure.proximity);
  float distance_infection_probability =
      DistanceToInfectionProbability(distance);

  return distance_infection_probability *
         std::log(1 -
                  exposure.infectivity *
                      absl::ToDoubleHours(exposure.duration) / 24.0f *
                      kSusceptibility * transmissibility +
                  kEpsilon);
}

}  // namespace

HealthTransition AggregatedTransmissionModel::GetInfectionOutcome(
    absl::Span<const Exposure* const> exposures) {
  absl::Time latest_exposure_time = absl::InfinitePast();
  float sum_exposures = 0.0f;
  for (const Exposure* exposure : exposures) {
    if (exposure->infectivity > 0) {
      latest_exposure_time = std::max(
          latest_exposure_time, exposure->start_time + exposure->duration);
      sum_exposures += ProbabilityExposureInfects(*exposure, transmissibility_);
    }
  }
  const float prob_infection = 1 - std::exp(sum_exposures);
  HealthTransition health_transition;
  health_transition.time = latest_exposure_time;
  health_transition.health_state = absl::Bernoulli(gen_, prob_infection)
                                       ? HealthState::EXPOSED
                                       : HealthState::SUSCEPTIBLE;
  return health_transition;
}

}  // namespace abesim
