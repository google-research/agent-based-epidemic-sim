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

#include "agent_based_epidemic_sim/agent_synthesis/agent_sampler.h"

namespace abesim {
namespace {
constexpr int64 kPopulationProfileId = 0;
}  // namespace

AgentProto ShuffledLocationAgentSampler::Next() {
  AgentProto agent;
  agent.set_uuid(uuid_generator_->GenerateUuid());
  agent.set_population_profile_id(kPopulationProfileId);
  agent.set_initial_health_state(health_state_sampler_->Sample().state());
  for (int i = 0; i < samplers_->size(); ++i) {
    const auto type = LocationProto::Type(i);
    if (!(*samplers_)[type].has_value()) {
      continue;
    }
    auto location = agent.add_locations();
    location->set_uuid((*samplers_)[type].value()->Next());
    location->set_type(type);
  }
  return agent;
}

}  // namespace abesim
