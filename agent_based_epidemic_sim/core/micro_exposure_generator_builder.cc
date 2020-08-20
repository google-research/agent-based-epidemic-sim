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

#include "agent_based_epidemic_sim/core/micro_exposure_generator_builder.h"

#include <memory>
#include <vector>

#include "absl/memory/memory.h"
#include "agent_based_epidemic_sim/core/constants.h"
#include "agent_based_epidemic_sim/core/event.h"
#include "agent_based_epidemic_sim/core/exposure_generator.h"
#include "agent_based_epidemic_sim/core/micro_exposure_generator.h"

namespace abesim {

std::unique_ptr<ExposureGenerator> MicroExposureGeneratorBuilder::Build(
    const std::vector<std::vector<float>>& proximity_trace_distribution) const {
  CHECK(!proximity_trace_distribution.empty())
      << "proximity_trace_distribution_ cannot be empty!";

  std::vector<ProximityTrace> fixed_length_proximity_trace_distribution;
  for (const auto& proximity_trace : proximity_trace_distribution) {
    const ProximityTrace fixed_length_proximity_trace(proximity_trace);
    fixed_length_proximity_trace_distribution.push_back(
        fixed_length_proximity_trace);
  }
  return absl::make_unique<MicroExposureGenerator>(
      fixed_length_proximity_trace_distribution);
}

}  // namespace abesim
