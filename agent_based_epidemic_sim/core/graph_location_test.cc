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
  ExposurePair Generate(const HostData& host_a, const HostData& host_b) const {
    return {.host_a = {.infectivity = host_b.infectivity},
            .host_b = {.infectivity = host_a.infectivity}};
  }
};

static constexpr int kLocationUUID = 1;

Visit GenerateVisit(int64 agent, HealthState::State health_state,
                    int random_location_edges = 1) {
  return {
      .location_uuid = kLocationUUID,
      .agent_uuid = agent,
      .health_state = health_state,
      .infectivity = (health_state == HealthState::INFECTIOUS) ? 1.0f : 0.0f,
      .symptom_factor = (health_state == HealthState::INFECTIOUS) ? 1.0f : 0.0f,
      .location_dynamics{
          .random_location_edges = random_location_edges,
      }};
}
InfectionOutcome ExpectedOutcome(int64 agent, int64 source, float infectivity,
                                 float transmissibility) {
  return {
      .agent_uuid = agent,
      .exposure =
          {
              .infectivity = infectivity,
              .location_transmissibility = transmissibility,
          },
      .exposure_type = InfectionOutcomeProto::CONTACT,
      .source_uuid = source,
  };
}

TEST(GraphLocationTest, CompleteSampleGenerated) {
  FakeExposureGenerator generator;
  auto location = NewGraphLocation(
      kLocationUUID, []() { return 0.75; }, 0.0,
      {{0, 2}, {0, 4}, {1, 3}, {1, 5}, {2, 4}, {3, 5}}, generator);
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
                                   ExpectedOutcome(0, 2, 1.0, 0.75),  //
                                   ExpectedOutcome(2, 0, 0.0, 0.75),  //
                                   ExpectedOutcome(0, 4, 0.0, 0.75),  //
                                   ExpectedOutcome(4, 0, 0.0, 0.75),  //
                                   ExpectedOutcome(1, 5, 1.0, 0.75),  //
                                   ExpectedOutcome(5, 1, 0.0, 0.75),  //
                                   ExpectedOutcome(2, 4, 0.0, 0.75),  //
                                   ExpectedOutcome(4, 2, 1.0, 0.75),  //
                               }));
}

TEST(GraphLocationTest, AllSamplesDropped) {
  FakeExposureGenerator generator;
  auto location = NewGraphLocation(
      kLocationUUID, []() { return 0.75; }, 1.0,
      {{0, 2}, {0, 4}, {1, 3}, {1, 5}, {2, 4}, {3, 5}}, generator);
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

TEST(AgentUuidsFromRandomLocationVisits, Basic) {
  std::vector<int64> agent_uuids;
  internal::AgentUuidsFromRandomLocationVisits(
      {
          GenerateVisit(0, HealthState::SUSCEPTIBLE, 2),
          GenerateVisit(1, HealthState::SUSCEPTIBLE, 3),
          GenerateVisit(2, HealthState::INFECTIOUS, 1),
          // Note: agent 3 does not make a visit.
          GenerateVisit(4, HealthState::SUSCEPTIBLE, 4),
          GenerateVisit(5, HealthState::INFECTIOUS, 2),
      },
      agent_uuids);
  EXPECT_THAT(agent_uuids, testing::UnorderedElementsAreArray(
                               {0, 0, 1, 1, 1, 2, 4, 4, 4, 4, 5, 5}));
}

TEST(AgentUuidsFromRandomLocationVisits, ClearsOutputArg) {
  // Tests that pre-existing values are removed from agent_uuids.
  std::vector<int64> agent_uuids = {1, 2, 3};
  internal::AgentUuidsFromRandomLocationVisits(
      {
          GenerateVisit(0, HealthState::SUSCEPTIBLE, 2),
          GenerateVisit(1, HealthState::SUSCEPTIBLE, 3),
          GenerateVisit(2, HealthState::INFECTIOUS, 1),
          // Note: agent 3 does not make a visit.
          GenerateVisit(4, HealthState::SUSCEPTIBLE, 4),
          GenerateVisit(5, HealthState::INFECTIOUS, 2),
      },
      agent_uuids);
  EXPECT_THAT(agent_uuids, testing::UnorderedElementsAreArray(
                               {0, 0, 1, 1, 1, 2, 4, 4, 4, 4, 5, 5}));
}

TEST(ConnectAdjacentNodes, Basic) {
  std::vector<std::pair<int64, int64>> graph;
  internal::ConnectAdjacentNodes({1, 2, 3, 4, 5, 6, 7}, graph);
  EXPECT_THAT(graph, testing::ElementsAreArray({
                         testing::Pair(1, 2),
                         testing::Pair(3, 4),
                         testing::Pair(5, 6),
                     }));
}

TEST(ConnectAdjacentNodes, EdgesAreSortedAndDistinct) {
  // Tests that the graph's edges are sorted and distinct.
  std::vector<std::pair<int64, int64>> graph;
  internal::ConnectAdjacentNodes({2, 1, 3, 1, 3, 4, 1, 2}, graph);
  EXPECT_THAT(graph, testing::ElementsAreArray({
                         testing::Pair(1, 2),
                         testing::Pair(1, 3),
                         testing::Pair(3, 4),
                     }));
}

TEST(ConnectAdjacentNodes, NoSelfEdges) {
  // Tests that the graph does not include self-edges.
  std::vector<std::pair<int64, int64>> graph;
  internal::ConnectAdjacentNodes({1, 1, 2, 3, 3, 4}, graph);
  EXPECT_THAT(graph, testing::ElementsAreArray({
                         testing::Pair(1, 2),
                         testing::Pair(3, 4),
                     }));
}

}  // namespace
}  // namespace abesim
