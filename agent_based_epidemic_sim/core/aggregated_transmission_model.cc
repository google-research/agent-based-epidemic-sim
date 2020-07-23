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
#include "absl/time/time.h"
#include "agent_based_epidemic_sim/core/constants.h"

namespace abesim {
namespace {
// TODO: Move into the visit message about the visiting agent.
constexpr float kSusceptibility = 1;
constexpr double kEpsilon = 1e-8;

// Used as a constant when computing a distance to infection probability.
// Details in (broken link).
constexpr float kDistanceInfectionProbabilityS = 1.5;

// Used as a constant when computing a distance to infection probability.
// Details in (broken link).
constexpr float kDistanceInfectionProbabilityB = 6.6;

constexpr std::array<float, kNumberMicroExposureBuckets> kIndexToMeters = {
    0.5, 1.5, 2.5, 3.5, 4.5, 5.5, 6.5, 7.5, 8.5, 9.5};

float DistanceToInfectionProbability(float distance) {
  // Infectiousness is defined as a sigmoid curve that starts at 100% at 0
  // distance, is 50% at 4 meters, and is 0% at 10 meters. These values are
  // estimated heuristically.
  return 1 - 1 / (1 + std::exp(-kDistanceInfectionProbabilityS * distance +
                               kDistanceInfectionProbabilityB));
}

// Returns distance in meters as a function of MicroExposure bucket index.
float ExtractDistanceFromIndex(size_t index) { return kIndexToMeters[index]; }

float ExposureHazard(const Exposure& exposure) {
  float hazard = 0;

  if (exposure.duration > absl::Minutes(1) &&
      exposure.micro_exposure_counts == kEmptyMicroExposures) {
    hazard = absl::ToInt64Minutes(exposure.duration);
  }

  for (size_t i = 0; i < exposure.micro_exposure_counts.size(); i++) {
    const uint8 duration_minutes = exposure.micro_exposure_counts[i];
    if (duration_minutes > 0) {
      const uint8 distance = ExtractDistanceFromIndex(i);

      hazard += DistanceToInfectionProbability(distance) * duration_minutes;
    }
  }
  return hazard * exposure.infectivity * exposure.symptom_factor;
}

}  // namespace

HealthTransition AggregatedTransmissionModel::GetInfectionOutcome(
    absl::Span<const Exposure* const> exposures) {
  absl::Time latest_exposure_time = absl::InfinitePast();
  float sum_exposure_hazards = 0.0f;
  for (const Exposure* exposure : exposures) {
    if (exposure->infectivity > 0) {
      latest_exposure_time = std::max(
          latest_exposure_time, exposure->start_time + exposure->duration);
      sum_exposure_hazards += ExposureHazard(*exposure);
    }
  }
  const float prob_infection = 1 - std::exp(-sum_exposure_hazards);
  HealthTransition health_transition;
  health_transition.time = latest_exposure_time;
  health_transition.health_state = absl::Bernoulli(gen_, prob_infection)
                                       ? HealthState::EXPOSED
                                       : HealthState::SUSCEPTIBLE;
  return health_transition;
}

}  // namespace abesim
