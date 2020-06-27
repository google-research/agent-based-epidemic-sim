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

#ifndef THIRD_PARTY_AGENT_BASED_EPIDEMIC_SIM_AGENT_SYNTHESIS_SHUFFLED_SAMPLER_H_
#define THIRD_PARTY_AGENT_BASED_EPIDEMIC_SIM_AGENT_SYNTHESIS_SHUFFLED_SAMPLER_H_

#include "absl/container/flat_hash_map.h"
#include "absl/random/random.h"
#include "agent_based_epidemic_sim/agent_synthesis/population_profile.pb.h"
#include "agent_based_epidemic_sim/core/enum_indexed_array.h"
#include "agent_based_epidemic_sim/core/integral_types.h"
#include "agent_based_epidemic_sim/core/parameter_distribution.pb.h"
#include "agent_based_epidemic_sim/core/uuid_generator.h"

namespace pandemic {

class ShuffledSampler {
 public:
  explicit ShuffledSampler(
      const absl::flat_hash_map<int64, int>& uuids_to_sizes);

  int64 Next();

 private:
  std::vector<int64> slots_;
  int i_ = 0;
};

// Constructs a distribution over business types.
// Samples businesses from the distribution until the total number of business
// "slots" exceeds the population size.
// Constructs a normalized size-weighted categorical distribution over
// businesses.
std::unique_ptr<ShuffledSampler> MakeBusinessSampler(
    const GammaDistribution& business_distribution, const int64 population_size,
    const UuidGenerator& uuid_generator, std::vector<LocationProto>* locations);

std::unique_ptr<ShuffledSampler> MakeHouseholdSampler(
    const DiscreteDistribution& household_distribution,
    const int64 population_size, const UuidGenerator& uuid_generator,
    std::vector<LocationProto>* locations);

}  // namespace pandemic

#endif  // THIRD_PARTY_AGENT_BASED_EPIDEMIC_SIM_AGENT_SYNTHESIS_SHUFFLED_SAMPLER_H_
