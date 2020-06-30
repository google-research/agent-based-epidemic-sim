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

#ifndef THIRD_PARTY_AGENT_BASED_EPIDEMIC_SIM_AGENT_SYNTHESIS_AGENT_SAMPLER_H_
#define THIRD_PARTY_AGENT_BASED_EPIDEMIC_SIM_AGENT_SYNTHESIS_AGENT_SAMPLER_H_

#include "absl/random/random.h"
#include "agent_based_epidemic_sim/agent_synthesis/population_profile.pb.h"
#include "agent_based_epidemic_sim/agent_synthesis/shuffled_sampler.h"
#include "agent_based_epidemic_sim/core/distribution_sampler.h"
#include "agent_based_epidemic_sim/core/enum_indexed_array.h"
#include "agent_based_epidemic_sim/core/pandemic.pb.h"
#include "agent_based_epidemic_sim/core/parameter_distribution.pb.h"
#include "agent_based_epidemic_sim/core/uuid_generator.h"

namespace abesim {

class AgentSampler {
 public:
  virtual AgentProto Next() = 0;
  virtual ~AgentSampler() = default;
};

using HealthStateSampler = DiscreteDistributionSampler<HealthState>;

using Samplers =
    EnumIndexedArray<absl::optional<std::unique_ptr<ShuffledSampler>>,
                     LocationProto::Type, LocationProto::Type_ARRAYSIZE>;

class ShuffledLocationAgentSampler : public AgentSampler {
 public:
  ShuffledLocationAgentSampler(
      std::unique_ptr<Samplers> samplers,
      std::unique_ptr<UuidGenerator> uuid_generator,
      std::unique_ptr<HealthStateSampler> health_state_sampler)
      : samplers_(std::move(samplers)),
        uuid_generator_(std::move(uuid_generator)),
        health_state_sampler_(std::move(health_state_sampler)) {}

  AgentProto Next() override;

 private:
  absl::BitGen gen_;
  std::unique_ptr<Samplers> samplers_;
  std::unique_ptr<UuidGenerator> uuid_generator_;
  std::unique_ptr<HealthStateSampler> health_state_sampler_;
};

}  // namespace abesim

#endif  // THIRD_PARTY_AGENT_BASED_EPIDEMIC_SIM_AGENT_SYNTHESIS_AGENT_SAMPLER_H_
