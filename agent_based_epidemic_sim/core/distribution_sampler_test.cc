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

#include "agent_based_epidemic_sim/core/distribution_sampler.h"

#include "agent_based_epidemic_sim/agent_synthesis/population.pb.h"
#include "agent_based_epidemic_sim/core/integral_types.h"
#include "agent_based_epidemic_sim/core/parameter_distribution.pb.h"
#include "agent_based_epidemic_sim/core/parse_text_proto.h"
#include "gmock/gmock.h"
#include "gtest/gtest.h"

namespace abesim {
namespace {

using ::testing::AnyOf;
using ::testing::ElementsAre;

TEST(DiscreteDistributionSamplerTest, CreatesSingleIntDistribution) {
  auto sampler = DiscreteDistributionSampler<int64>::FromProto(
      ParseTextProtoOrDie<DiscreteDistribution>(R"pb(
        buckets { count: 1 int_value: 1 }
        buckets { count: 2 int_value: 10 }
        buckets { count: 7 int_value: 100 }
      )pb"));

  EXPECT_THAT(sampler->GetProbabilities(), ElementsAre(0.1, 0.2, 0.7));
  EXPECT_THAT(sampler->GetValues(), ElementsAre(1, 10, 100));
  EXPECT_THAT(sampler->Sample(), AnyOf(1, 10, 100));
}

MATCHER_P(EqualsPerson, person, "") {
  return arg.age() == person.age() && arg.sex() == person.sex();
}

TEST(DiscreteDistributionSamplerTest, CreatesSingleProtoDistribution) {
  Person person_a = ParseTextProtoOrDie<Person>(R"pb(
    age: 50 sex: FEMALE
  )pb");
  Person person_b = ParseTextProtoOrDie<Person>(R"pb(
    age: 40 sex: MALE
  )pb");
  DiscreteDistribution dist = ParseTextProtoOrDie<DiscreteDistribution>(R"pb(
    buckets { count: 1 }
    buckets { count: 3 }
  )pb");
  dist.mutable_buckets(0)->mutable_proto_value()->PackFrom(person_a);
  dist.mutable_buckets(1)->mutable_proto_value()->PackFrom(person_b);

  auto sampler = DiscreteDistributionSampler<Person>::FromProto(dist);

  EXPECT_THAT(sampler->GetProbabilities(), ElementsAre(0.25, 0.75));
  EXPECT_THAT(sampler->GetValues(),
              ElementsAre(EqualsPerson(person_a), EqualsPerson(person_b)));
  EXPECT_THAT(sampler->Sample(),
              AnyOf(EqualsPerson(person_a), EqualsPerson(person_b)));
}

}  // namespace
}  // namespace abesim
