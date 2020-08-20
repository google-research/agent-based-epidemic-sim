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

#include "agent_based_epidemic_sim/core/location_discrete_event_simulator_builder.h"

#include "agent_based_epidemic_sim/core/constants.h"
#include "agent_based_epidemic_sim/core/location_discrete_event_simulator.h"
#include "agent_based_epidemic_sim/core/micro_exposure_generator_builder.h"

namespace abesim {

std::unique_ptr<Location> LocationDiscreteEventSimulatorBuilder::Build() const {
  MicroExposureGeneratorBuilder meg_builder;
  return absl::make_unique<LocationDiscreteEventSimulator>(
      uuid_generator_->GenerateUuid(),
      meg_builder.Build(kNonParametricTraceDistribution));
}

}  // namespace abesim
