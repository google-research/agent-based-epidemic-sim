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

#include "agent_based_epidemic_sim/agent_synthesis/shuffled_sampler.h"

#include "agent_based_epidemic_sim/core/distribution_sampler.h"
#include "agent_based_epidemic_sim/core/random.h"
#include "agent_based_epidemic_sim/port/logging.h"

namespace abesim {

ShuffledSampler::ShuffledSampler(
    const absl::flat_hash_map<int64_t, int>& uuids_to_sizes) {
  for (auto key_val : uuids_to_sizes) {
    for (int i = 0; i < key_val.second; ++i) {
      slots_.push_back(key_val.first);
    }
  }
  std::shuffle(slots_.begin(), slots_.end(), GetBitGen());
}

int64_t ShuffledSampler::Next() {
  DCHECK(i_ < slots_.size());
  return slots_[i_++];
}

std::unique_ptr<ShuffledSampler> MakeBusinessSampler(
    const GammaDistribution& business_distribution,
    const int64_t population_size, const UuidGenerator& uuid_generator,
    std::vector<LocationProto>* locations) {
  auto business_size_distribution = std::gamma_distribution<float>(
      business_distribution.alpha(), business_distribution.beta());
  std::mt19937 rng;
  absl::flat_hash_map<int64_t, int> uuid_to_sizes;
  for (int population = 0; population < population_size;) {
    LocationProto location;
    location.mutable_reference()->set_uuid(uuid_generator.GenerateUuid());
    location.mutable_reference()->set_type(LocationReference::BUSINESS);
    const int size =
        std::min(static_cast<int64_t>(business_size_distribution(rng)),
                 population_size - population);
    location.mutable_dense()->set_size(size);
    locations->push_back(location);
    uuid_to_sizes.insert(
        std::make_pair(locations->back().reference().uuid(), size));
    population += size;
  }
  return absl::make_unique<ShuffledSampler>(uuid_to_sizes);
}

std::unique_ptr<ShuffledSampler> MakeHouseholdSampler(
    const DiscreteDistribution& household_distribution,
    const int64_t population_size, const UuidGenerator& uuid_generator,
    std::vector<LocationProto>* locations) {
  auto household_size_sampler =
      DiscreteDistributionSampler<int64_t>::FromProto(household_distribution);
  absl::flat_hash_map<int64_t, int> uuid_to_sizes;
  for (int population = 0; population < population_size;) {
    LocationProto location;
    location.mutable_reference()->set_uuid(uuid_generator.GenerateUuid());
    location.mutable_reference()->set_type(LocationReference::HOUSEHOLD);
    locations->push_back(location);
    const int size = std::min(household_size_sampler->Sample(),
                              population_size - population);
    location.mutable_dense()->set_size(size);
    uuid_to_sizes.insert(
        std::make_pair(locations->back().reference().uuid(), size));
    population += size;
  }
  return absl::make_unique<ShuffledSampler>(uuid_to_sizes);
}

}  // namespace abesim
