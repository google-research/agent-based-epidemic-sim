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

#include "agent_based_epidemic_sim/core/risk_score.h"

#include "absl/time/time.h"
#include "agent_based_epidemic_sim/applications/home_work/config.pb.h"
#include "agent_based_epidemic_sim/applications/home_work/risk_score.h"
#include "agent_based_epidemic_sim/port/status_matchers.h"
#include "agent_based_epidemic_sim/port/time_proto_util.h"
#include "agent_based_epidemic_sim/util/ostream_overload.h"
#include "gmock/gmock.h"
#include "gtest/gtest.h"

namespace abesim {
namespace {

absl::Time TestDay(int day) {
  return absl::UnixEpoch() + absl::Hours(24) * day;
}

std::vector<float> FrequencyAdjustments(
    const ToggleRiskScoreGenerator* const gen, const float essentialness,
    const LocationType type, const std::vector<int>& days) {
  int64 location_uuid = type == LocationType::kWork ? 0 : 1;
  auto risk_score = gen->GetRiskScore(essentialness);

  std::vector<float> adjustments;
  for (const int day : days) {
    Timestep timestep(TestDay(day), absl::Hours(24));
    adjustments.push_back(
        risk_score->GetVisitAdjustment(timestep, location_uuid)
            .frequency_adjustment);
  }
  return adjustments;
}

DistancingPolicy BuildPolicy(std::vector<std::pair<int, float>> stages) {
  DistancingPolicy policy;
  for (const std::pair<int, float>& stage : stages) {
    DistancingStageProto* new_stage = policy.add_stages();
    CHECK_EQ(absl::OkStatus(),
             EncodeGoogleApiProto(TestDay(stage.first),
                                  new_stage->mutable_start_time()));
    new_stage->set_essential_worker_fraction(stage.second);
  }
  return policy;
}

OVERLOAD_VECTOR_OSTREAM_OPS

struct Case {
  float essentialness;
  LocationType type;
  std::vector<float> expected_frequency_adjustments;

  friend std::ostream& operator<<(std::ostream& strm, const Case& c) {
    return strm << "{" << c.essentialness << ", " << static_cast<int>(c.type)
                << ", " << c.expected_frequency_adjustments << "}";
  }
};

TEST(PublicPolicyTest, AppropriateFrequencyAdjustments) {
  DistancingPolicy config =
      BuildPolicy({{10, .6}, {3, .2}, {20, 1.0}, {15, .2}});
  auto generator_or =
      NewRiskScoreGenerator(config, [](const int64 location_uuid) {
        return location_uuid == 0 ? LocationType::kWork : LocationType::kHome;
      });
  PANDEMIC_ASSERT_OK(generator_or);
  ToggleRiskScoreGenerator* gen = generator_or->get();

  std::vector<int> test_days = {1, 3, 5, 10, 15, 20, 25};
  Case cases[] = {
      {0.0, LocationType::kWork, {1.0, 1.0, 1.0, 1.0, 1.0, 1.0, 1.0}},
      {0.1, LocationType::kWork, {1.0, 1.0, 1.0, 1.0, 1.0, 1.0, 1.0}},
      {0.2, LocationType::kWork, {1.0, 1.0, 1.0, 1.0, 1.0, 1.0, 1.0}},
      {0.21, LocationType::kWork, {1.0, 0.0, 0.0, 1.0, 0.0, 1.0, 1.0}},
      {0.6, LocationType::kWork, {1.0, 0.0, 0.0, 1.0, 0.0, 1.0, 1.0}},
      {0.61, LocationType::kWork, {1.0, 0.0, 0.0, 0.0, 0.0, 1.0, 1.0}},
      {0.9, LocationType::kWork, {1.0, 0.0, 0.0, 0.0, 0.0, 1.0, 1.0}},
      {1.0, LocationType::kWork, {1.0, 0.0, 0.0, 0.0, 0.0, 1.0, 1.0}},
      // We always go to home locations.
      {0.0, LocationType::kHome, {1.0, 1.0, 1.0, 1.0, 1.0, 1.0, 1.0}},
      {0.1, LocationType::kHome, {1.0, 1.0, 1.0, 1.0, 1.0, 1.0, 1.0}},
      {0.2, LocationType::kHome, {1.0, 1.0, 1.0, 1.0, 1.0, 1.0, 1.0}},
      {0.21, LocationType::kHome, {1.0, 1.0, 1.0, 1.0, 1.0, 1.0, 1.0}},
      {0.6, LocationType::kHome, {1.0, 1.0, 1.0, 1.0, 1.0, 1.0, 1.0}},
      {0.61, LocationType::kHome, {1.0, 1.0, 1.0, 1.0, 1.0, 1.0, 1.0}},
      {0.9, LocationType::kHome, {1.0, 1.0, 1.0, 1.0, 1.0, 1.0, 1.0}},
      {1.0, LocationType::kHome, {1.0, 1.0, 1.0, 1.0, 1.0, 1.0, 1.0}},
  };
  for (const Case& c : cases) {
    EXPECT_THAT(FrequencyAdjustments(gen, c.essentialness, c.type, test_days),
                testing::ElementsAreArray(c.expected_frequency_adjustments))
        << c;
  }
}

TEST(PublicPolicyTest, ZeroStagePolicy) {
  DistancingPolicy config;
  auto generator_or =
      NewRiskScoreGenerator(config, [](const int64 location_uuid) {
        return location_uuid == 0 ? LocationType::kWork : LocationType::kHome;
      });
  ToggleRiskScoreGenerator* gen = generator_or->get();
  std::vector<int> test_days = {1, 3, 5, 10, 15, 20, 25};
  Case cases[] = {
      {1.0, LocationType::kWork, {1.0, 1.0, 1.0, 1.0, 1.0, 1.0, 1.0}},
      {1.0, LocationType::kHome, {1.0, 1.0, 1.0, 1.0, 1.0, 1.0, 1.0}},
  };
  for (const Case& c : cases) {
    EXPECT_THAT(FrequencyAdjustments(gen, c.essentialness, c.type, test_days),
                testing::ElementsAreArray(c.expected_frequency_adjustments))
        << c;
  }
}

}  // namespace
}  // namespace abesim
