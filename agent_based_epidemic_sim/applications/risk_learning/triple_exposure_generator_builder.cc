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

#include "agent_based_epidemic_sim/applications/risk_learning/triple_exposure_generator_builder.h"

#include <memory>
#include <random>

#include "absl/memory/memory.h"
#include "absl/time/time.h"
#include "agent_based_epidemic_sim/applications/risk_learning/config.pb.h"
#include "agent_based_epidemic_sim/applications/risk_learning/triple_exposure_generator.h"
#include "agent_based_epidemic_sim/core/exposure_generator.h"
#include "agent_based_epidemic_sim/core/parameter_distribution.pb.h"

namespace abesim {

std::unique_ptr<ExposureGenerator> TripleExposureGeneratorBuilder::Build()
    const {
  DistanceGammaDistributionParams distance_params;
  if (proto_.has_distance_distribution()) {
    GammaDistribution distance_distribution = proto_.distance_distribution();
    distance_params.shape = distance_distribution.alpha();
    distance_params.scale = distance_distribution.beta();
  }

  DurationParetoDistributionParams duration_params;
  if (proto_.has_duration_distribution()) {
    DurationDistribution duration_distribution = proto_.duration_distribution();
    if (duration_distribution.has_pareto_params()) {
      duration_params.shape = duration_distribution.pareto_params().shape();
      duration_params.scale = duration_distribution.pareto_params().scale();
    }
    duration_params.output_multiplier_minutes =
        absl::Minutes(1) * duration_distribution.output_multiplier_minutes();
  }

  BleParams ble_params;
  if (proto_.has_ble_params()) {
    BleParamsProto ble_params_proto = proto_.ble_params();
    ble_params.correction = ble_params_proto.correction();
    ble_params.intercept = ble_params_proto.intercept();
    ble_params.slope = ble_params_proto.slope();
    ble_params.tx = ble_params_proto.tx();
  }

  return absl::make_unique<TripleExposureGenerator>(
      distance_params, duration_params, ble_params);
}
}  // namespace abesim
