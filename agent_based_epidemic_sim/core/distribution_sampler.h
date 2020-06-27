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

#ifndef THIRD_PARTY_AGENT_BASED_EPIDEMIC_SIM_CORE_DISTRIBUTION_SAMPLER_H_
#define THIRD_PARTY_AGENT_BASED_EPIDEMIC_SIM_CORE_DISTRIBUTION_SAMPLER_H_

#include <functional>
#include <memory>
#include <random>

#include "absl/container/flat_hash_map.h"
#include "absl/random/discrete_distribution.h"
#include "absl/random/random.h"
#include "agent_based_epidemic_sim/core/integral_types.h"
#include "agent_based_epidemic_sim/core/parameter_distribution.pb.h"
#include "agent_based_epidemic_sim/port/logging.h"
#include "google/protobuf/any.pb.h"

namespace pandemic {

// Creates a sampler based on non-uniform discrete distributions.
// Assumes the number of buckets is relatively small.
template <typename T>
class DiscreteDistributionSampler {
 public:
  // Returns a value sampled from the distribution.
  T Sample() { return values_[distribution_(gen_)]; }

  // Creates a DiscreteDistributionSampler from the given distribution.
  static std::unique_ptr<DiscreteDistributionSampler<T>> FromProto(
      const DiscreteDistribution& dist);

  // Returns the associated probabilities of the distribution.
  const std::vector<double> GetProbabilities() {
    return distribution_.probabilities();
  }

  // Returns the values for the distribution buckets.
  const std::vector<T> GetValues() { return values_; }

 private:
  DiscreteDistributionSampler(std::vector<T> values,
                              absl::discrete_distribution<int> distribution)
      : values_(std::move(values)), distribution_(std::move(distribution)) {}
  static auto ValueGetter();

  absl::BitGen gen_;
  const std::vector<T> values_;
  absl::discrete_distribution<int> distribution_;
};

template <typename T>
std::unique_ptr<DiscreteDistributionSampler<T>>
DiscreteDistributionSampler<T>::FromProto(
    const DiscreteDistribution& dist_proto) {
  const auto value_getter = ValueGetter();

  std::vector<double> probabilities;
  probabilities.reserve(dist_proto.buckets().size());
  for (const auto& bucket : dist_proto.buckets()) {
    probabilities.push_back(bucket.count());
  }
  std::vector<T> values;
  values.reserve(dist_proto.buckets().size());
  for (const auto& bucket : dist_proto.buckets()) {
    values.push_back(value_getter(bucket));
  }
  absl::discrete_distribution<int> dist(probabilities.begin(),
                                        probabilities.end());

  return absl::WrapUnique(
      new DiscreteDistributionSampler<T>(values, std::move(dist)));
}

template <typename T>
auto DiscreteDistributionSampler<T>::ValueGetter() {
  return [](const DiscreteDistribution::Bucket& bucket) {
    CHECK(bucket.value_case() == DiscreteDistribution_Bucket::kProtoValue)
        << "Inconsistent DiscreteDistribution bucket value found, expected "
           "proto_value.";
    T proto;
    bucket.proto_value().UnpackTo(&proto);
    return proto;
  };
}

template <>
inline auto DiscreteDistributionSampler<int64>::ValueGetter() {
  return [](const DiscreteDistribution::Bucket& bucket) {
    CHECK(bucket.value_case() == DiscreteDistribution_Bucket::kIntValue)
        << "Inconsistent DiscreteDistribution bucket value found, expected "
           "int_value.";
    return bucket.int_value();
  };
}

template <>
inline auto DiscreteDistributionSampler<std::string>::ValueGetter() {
  return [](const DiscreteDistribution::Bucket& bucket) {
    CHECK(bucket.value_case() == DiscreteDistribution_Bucket::kStringValue)
        << "Inconsistent DiscreteDistribution bucket value found, expected "
           "string_value.";
    return bucket.string_value();
  };
}

}  // namespace pandemic

#endif  // THIRD_PARTY_AGENT_BASED_EPIDEMIC_SIM_CORE_DISTRIBUTION_SAMPLER_H_
