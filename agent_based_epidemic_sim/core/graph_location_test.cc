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

#include "agent_based_epidemic_sim/core/graph_location.h"

#include <memory>
#include <utility>

#include "agent_based_epidemic_sim/core/event.h"
#include "agent_based_epidemic_sim/core/exposure_generator.h"
#include "agent_based_epidemic_sim/core/pandemic.pb.h"
#include "gmock/gmock.h"
#include "gtest/gtest.h"

namespace abesim {
namespace {

class FakeBroker : public Broker<InfectionOutcome> {
 public:
  void Send(const absl::Span<const InfectionOutcome> msgs) override {
    visits_.insert(visits_.end(), msgs.begin(), msgs.end());
  }

  const std::vector<InfectionOutcome>& visits() const { return visits_; }

 private:
  std::vector<InfectionOutcome> visits_;
};

class FakeExposureGenerator : public ExposureGenerator {
  ExposurePair Generate(const HostData& host_a, const HostData& host_b) {
    return {.host_a = {.infectivity = host_b.infectivity},
            .host_b = {.infectivity = host_a.infectivity}};
  }
};

static constexpr int kLocationUUID = 1;

Visit GenerateVisit(int64 agent, HealthState::State health_state) {
  return {
      .location_uuid = kLocationUUID,
      .agent_uuid = agent,
      .health_state = health_state,
      .infectivity = (health_state == HealthState::INFECTIOUS) ? 1.0f : 0.0f,
      .symptom_factor = (health_state == HealthState::INFECTIOUS) ? 1.0f : 0.0f,
  };
}
InfectionOutcome ExpectedOutcome(int64 agent, int64 source, float infectivity) {
  return {
      .agent_uuid = agent,
      .exposure =
          {
              .infectivity = infectivity,
          },
      .exposure_type = InfectionOutcomeProto::CONTACT,
      .source_uuid = source,
  };
}

TEST(GraphLocationTest, CompleteSampleGenerated) {
  auto location = NewGraphLocation(
      kLocationUUID, 0.0, {{0, 2}, {0, 4}, {1, 3}, {1, 5}, {2, 4}, {3, 5}},
      absl::Hours(8), absl::Hours(2),
      std::make_unique<FakeExposureGenerator>());
  FakeBroker broker;
  location->ProcessVisits(
      {
          GenerateVisit(0, HealthState::SUSCEPTIBLE),
          GenerateVisit(1, HealthState::SUSCEPTIBLE),
          GenerateVisit(2, HealthState::INFECTIOUS),
          // Note: agent 3 does not make a visit.
          GenerateVisit(4, HealthState::SUSCEPTIBLE),
          GenerateVisit(5, HealthState::INFECTIOUS),
      },
      &broker);
  EXPECT_THAT(broker.visits(), testing::UnorderedElementsAreArray({
                                   ExpectedOutcome(0, 2, 1.0),  //
                                   ExpectedOutcome(2, 0, 0.0),  //
                                   ExpectedOutcome(0, 4, 0.0),  //
                                   ExpectedOutcome(4, 0, 0.0),  //
                                   ExpectedOutcome(1, 5, 1.0),  //
                                   ExpectedOutcome(5, 1, 0.0),  //
                                   ExpectedOutcome(2, 4, 0.0),  //
                                   ExpectedOutcome(4, 2, 1.0),  //
                               }));
}

TEST(GraphLocationTest, AllSamplesDropped) {
  auto location = NewGraphLocation(
      kLocationUUID, 1.0, {{0, 2}, {0, 4}, {1, 3}, {1, 5}, {2, 4}, {3, 5}},
      absl::Hours(8), absl::Hours(2),
      std::make_unique<FakeExposureGenerator>());
  FakeBroker broker;
  location->ProcessVisits(
      {
          GenerateVisit(0, HealthState::SUSCEPTIBLE),
          GenerateVisit(1, HealthState::SUSCEPTIBLE),
          GenerateVisit(2, HealthState::INFECTIOUS),
          // Note: agent 3 does not make a visit.
          GenerateVisit(4, HealthState::SUSCEPTIBLE),
          GenerateVisit(5, HealthState::INFECTIOUS),
      },
      &broker);
  EXPECT_TRUE(broker.visits().empty());
}

}  // namespace
}  // namespace abesim
