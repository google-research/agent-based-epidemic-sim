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

#ifndef AGENT_BASED_EPIDEMIC_SIM_CORE_TRIPLE_EXPOSURE_GENERATOR_H_
#define AGENT_BASED_EPIDEMIC_SIM_CORE_TRIPLE_EXPOSURE_GENERATOR_H_

#include <random>

#include "absl/time/time.h"
#include "agent_based_epidemic_sim/core/exposure_generator.h"
namespace abesim {

// Default parameter values are MLEs derived from smoothed Copenhagen traces.
struct DistanceGammaDistributionParams {
  float shape = 1.472;
  float scale = 1.898;
};

// Default parameter values are MLEs derived from smoothed Copenhagen traces.
struct DurationParetoDistributionParams {
  float shape = 1.510;
  float scale = 1.0;
  absl::Duration output_multiplier_minutes = absl::Minutes(5);
};

// Inferring proximity from Bluetooth Low Energy RSSI with Unscented Kalman
// Smoothers, Tom Lovett, Mark Briers, Marcos Charalambides, Radka Jersakova,
// James Lomax, Chris Holmes, July 2020. https://arxiv.org/abs/2007.05057
struct BleParams {
  float slope = 0.21;
  float intercept = 3.92;
  float tx = 0.0;
  float correction = 2.398;
};

class TripleExposureGenerator : public ExposureGenerator {
 public:
  explicit TripleExposureGenerator(
      const DistanceGammaDistributionParams& distance_params =
          DistanceGammaDistributionParams(),
      const DurationParetoDistributionParams& duration_params =
          DurationParetoDistributionParams(),
      const BleParams& ble_params = BleParams())
      : distance_params_(distance_params),
        duration_params_(duration_params),
        ble_params_(ble_params) {}

  virtual ~TripleExposureGenerator() = default;
  // Generate a pair of Exposure objects representing a single "contact" between
  // two hosts.
  ExposurePair Generate(float location_transmissibility, const Visit& visit_a,
                        const Visit& visit_b) const override;

 private:
  // Draws a duration from a Pareto distribution using duration_params_.
  absl::Duration DrawDuration() const;

  // Draws a distance from a Gamma distribution using distance_params_.
  float DrawDistance() const;

  // Converts distance in meters to an attenuation value.
  //
  // Inferring proximity from Bluetooth Low Energy RSSI with Unscented Kalman
  // Smoothers, Tom Lovett, Mark Briers, Marcos Charalambides, Radka Jersakova,
  // James Lomax, Chris Holmes, July 2020. https://arxiv.org/abs/2007.05057
  float DistanceToAttenuation(float distance) const;

  const DistanceGammaDistributionParams& distance_params_;
  const DurationParetoDistributionParams& duration_params_;
  const BleParams& ble_params_;
};

}  // namespace abesim

#endif  // AGENT_BASED_EPIDEMIC_SIM_CORE_TRIPLE_EXPOSURE_GENERATOR_H_
