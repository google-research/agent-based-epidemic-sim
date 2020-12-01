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

#include "agent_based_epidemic_sim/applications/risk_learning/triple_exposure_generator.h"

#include <cmath>
#include <random>

#include "absl/base/optimization.h"
#include "absl/flags/flag.h"
#include "absl/random/distributions.h"
#include "absl/random/random.h"
#include "absl/time/time.h"
#include "agent_based_epidemic_sim/core/constants.h"
#include "agent_based_epidemic_sim/core/exposure_generator.h"
#include "agent_based_epidemic_sim/core/random.h"

ABSL_FLAG(bool, test_triple_exposure_generator_fixed_distance, false,
          "Whether the distance between agents at the time of an exposure "
          "should be fixed to the mean of the given Gamma distribution.");
ABSL_FLAG(bool, test_triple_exposure_generator_fixed_duration, false,
          "Whether the duration of interaction between agents at the time of "
          "an exposure should be fixed to the mean of the given distribution.");

namespace abesim {

float TripleExposureGenerator::DrawDistance() const {
  if (ABSL_PREDICT_TRUE(!absl::GetFlag(
          FLAGS_test_triple_exposure_generator_fixed_distance))) {
    absl::BitGenRef bitgen = GetBitGen();
    std::gamma_distribution<float> distance_distribution(
        distance_params_.shape, distance_params_.scale);
    return distance_distribution(bitgen);
  }
  return distance_params_.shape * distance_params_.scale;  // Mean.
}

absl::Duration TripleExposureGenerator::DrawDuration() const {
  if (ABSL_PREDICT_TRUE(!absl::GetFlag(
          FLAGS_test_triple_exposure_generator_fixed_duration))) {
    const float u = absl::Uniform(GetBitGen(), 0.0, 1.0);
    const float duration_intervals =
        duration_params_.shape / std::pow(u, 1 / duration_params_.scale);
    return duration_intervals * duration_params_.output_multiplier_minutes;
  }
  return duration_params_.output_multiplier_minutes * duration_params_.shape *
         duration_params_.scale / (duration_params_.shape - 1);  // Mean.
}

float TripleExposureGenerator::DistanceToAttenuation(float distance) const {
  // If distance is held fixed by setting
  // FLAGS_test_triple_exposure_generator_fixed_distance, then so is
  // attenuation, since it is a deterministic function of distance.
  const float mu =
      ble_params_.intercept + ble_params_.slope * std::log(distance);
  const float rssi = -std::exp(mu);
  const float attenuation = ble_params_.tx - (rssi + ble_params_.correction);
  return attenuation;
}

ExposurePair TripleExposureGenerator::Generate(float location_transmissibility,
                                               const Visit& visit_a,
                                               const Visit& visit_b) const {
  const absl::Duration duration = DrawDuration();
  const float distance = DrawDistance();
  const float attenuation = DistanceToAttenuation(distance);
  absl::Time start_time = std::max(visit_a.start_time, visit_b.start_time);
  return {.host_a = {.start_time = start_time,
                     .duration = duration,
                     .distance = distance,
                     .attenuation = attenuation,
                     .infectivity = visit_b.infectivity,
                     .symptom_factor = visit_b.symptom_factor,
                     .susceptibility = visit_a.susceptibility},
          .host_b = {.start_time = start_time,
                     .duration = duration,
                     .distance = distance,
                     .attenuation = attenuation,
                     .infectivity = visit_a.infectivity,
                     .symptom_factor = visit_a.symptom_factor,
                     .susceptibility = visit_b.susceptibility}};
}

}  // namespace abesim
