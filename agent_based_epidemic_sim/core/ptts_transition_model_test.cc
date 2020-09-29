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

#include "agent_based_epidemic_sim/core/ptts_transition_model.h"

#include "absl/container/flat_hash_map.h"
#include "absl/container/flat_hash_set.h"
#include "absl/time/time.h"
#include "agent_based_epidemic_sim/core/event.h"
#include "agent_based_epidemic_sim/core/pandemic.pb.h"
#include "agent_based_epidemic_sim/core/parse_text_proto.h"
#include "agent_based_epidemic_sim/core/visit.h"
#include "gmock/gmock.h"
#include "gtest/gtest.h"

namespace abesim {
namespace {

MATCHER_P(IsCloseTo, duration, "") {
  return arg == duration ||  // Check for equality to handle infinity.
         (arg > duration - absl::Hours(24) && arg < duration + absl::Hours(24));
}

TEST(PTTSTransitionModelTest, UpdatesTransitionModel) {
  auto model = PTTSTransitionModel::CreateFromProto(
      ParseTextProtoOrDie<PTTSTransitionModelProto>(R"pb(
        state_transition_diagram {
          health_state: EXPOSED
          transition_probability {
            health_state: PRE_SYMPTOMATIC_MILD
            transition_probability: 0.5
            mean_days_to_transition: 3.0
            sd_days_to_transition: 0.1
          }
          transition_probability {
            health_state: PRE_SYMPTOMATIC_SEVERE
            transition_probability: 0.5
            mean_days_to_transition: 6.0
            sd_days_to_transition: 0.1
          }
        }
        state_transition_diagram {
          health_state: PRE_SYMPTOMATIC_MILD
          transition_probability {
            health_state: SYMPTOMATIC_MILD
            transition_probability: 1.0
            mean_days_to_transition: 8.0
            sd_days_to_transition: 0.1
          }
        }
        state_transition_diagram {
          health_state: SYMPTOMATIC_MILD
          transition_probability {
            health_state: RECOVERED
            transition_probability: 1.0
            mean_days_to_transition: 12.0
            sd_days_to_transition: 0.1
          }
        }
      )pb"));

  struct TestCase {
    HealthState::State src;
    absl::flat_hash_map<HealthState::State, absl::Duration> expected;
  };
  std::vector<TestCase> cases = {
      {.src = HealthState::EXPOSED,
       .expected = {{HealthState::PRE_SYMPTOMATIC_MILD, 3 * absl::Hours(24)},
                    {HealthState::PRE_SYMPTOMATIC_SEVERE,
                     6 * absl::Hours(24)}}},
      {.src = HealthState::PRE_SYMPTOMATIC_MILD,
       .expected = {{HealthState::SYMPTOMATIC_MILD, 8 * absl::Hours(24)}}},
      {.src = HealthState::SYMPTOMATIC_MILD,
       .expected = {{HealthState::RECOVERED, 12 * absl::Hours(24)}}},
      // States that don't have out edges should self transition in infinite
      // time.
      {.src = HealthState::INFECTIOUS,
       .expected = {{HealthState::INFECTIOUS, absl::InfiniteDuration()}}},
  };
  for (const TestCase& test_case : cases) {
    // Run each test case several times.
    for (int i = 0; i < 10; ++i) {
      // Keep drawing until we've matched every expected destination state.
      absl::flat_hash_set<HealthState::State> found;
      while (found.size() < test_case.expected.size()) {
        HealthTransition transition = model->GetNextHealthTransition(
            {.time = absl::UnixEpoch(), .health_state = test_case.src});
        auto duration = test_case.expected.find(transition.health_state);
        ASSERT_TRUE(duration != test_case.expected.end())
            << " Unexpected transition from " << test_case.src << " to "
            << transition.health_state;
        ASSERT_THAT(transition.time - absl::UnixEpoch(),
                    IsCloseTo(duration->second))
            << " wrong duration for transition from " << test_case.src << " to "
            << transition.health_state;
        found.insert(transition.health_state);
      }
    }
  }
}

}  // namespace
}  // namespace abesim
