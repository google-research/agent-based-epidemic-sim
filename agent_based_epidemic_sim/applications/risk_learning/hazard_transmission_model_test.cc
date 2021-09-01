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

#include "agent_based_epidemic_sim/applications/risk_learning/hazard_transmission_model.h"

#include "absl/random/mock_distributions.h"
#include "absl/random/mocking_bit_gen.h"
#include "absl/time/time.h"
#include "agent_based_epidemic_sim/core/event.h"
#include "agent_based_epidemic_sim/core/visit.h"
#include "agent_based_epidemic_sim/util/test_util.h"
#include "gmock/gmock.h"
#include "gtest/gtest.h"

namespace abesim {
namespace {

using testing::Eq;
const absl::Duration kLongDuration = absl::Minutes(100);
const absl::Duration kShortDuration = absl::Minutes(1);

const float kCloseDistance = 1;
const float kFarDistance = 10;

TEST(HazardTransmissionModelTest, GetsInfectionOutcomes) {
  HazardTransmissionModel transmission_model(
      {.risk_at_distance_function = [](float distance) {
        return (distance <= 1) ? 10 : 0;
      }});

  std::vector<Exposure> exposures{{
                                      .duration = kShortDuration,
                                      .distance = kFarDistance,
                                      .infectivity = 1,
                                      .symptom_factor = 1,
                                      .susceptibility = 1,
                                      .location_transmissibility = 1,
                                  },
                                  {
                                      .duration = kShortDuration,
                                      .distance = kCloseDistance,
                                      .infectivity = 1,
                                      .symptom_factor = 1,
                                      .susceptibility = 1,
                                      .location_transmissibility = 1,
                                  },
                                  {
                                      .duration = kLongDuration,
                                      .distance = kCloseDistance,
                                      .infectivity = 1,
                                      .symptom_factor = 1,
                                      .susceptibility = 1,
                                      .location_transmissibility = 1,
                                  }};

  EXPECT_THAT(transmission_model.GetInfectionOutcome(MakePointers(exposures)),
              Eq(HealthTransition{.time = absl::UnixEpoch() + kLongDuration,
                                  .health_state = HealthState::EXPOSED}));

  exposures = {{
                   .duration = kShortDuration,
                   .distance = kFarDistance,
                   .infectivity = 1,
                   .symptom_factor = 1,
                   .susceptibility = 1,
                   .location_transmissibility = 1,
               },
               {
                   .duration = kShortDuration,
                   .distance = kCloseDistance,
                   .infectivity = 1,
                   .symptom_factor = 1,
                   .susceptibility = 1,
                   .location_transmissibility = 1,
               }};
  EXPECT_THAT(transmission_model.GetInfectionOutcome(MakePointers(exposures)),
              Eq(HealthTransition{.time = absl::UnixEpoch() + kShortDuration,
                                  .health_state = HealthState::EXPOSED}));

  exposures = {{
      .duration = kShortDuration,
      .distance = kFarDistance,
      .infectivity = 1,
      .symptom_factor = 1,
      .susceptibility = 1,
      .location_transmissibility = 1,
  }};
  EXPECT_THAT(transmission_model.GetInfectionOutcome(MakePointers(exposures)),
              Eq(HealthTransition{.time = absl::UnixEpoch() + kShortDuration,
                                  .health_state = HealthState::SUSCEPTIBLE}));
}

TEST(HazardTransmissionModelTest, GetsInfectionOutcomesWithCloseDistance) {
  HazardTransmissionModel transmission_model;

  std::vector<Exposure> exposures{{
      .duration = kLongDuration,
      .distance = kCloseDistance,
      .infectivity = 1,
      .symptom_factor = 1,
      .susceptibility = 1,
      .location_transmissibility = 1,
  }};

  EXPECT_THAT(transmission_model.GetInfectionOutcome(MakePointers(exposures)),
              Eq(HealthTransition{.time = absl::UnixEpoch() + kLongDuration,
                                  .health_state = HealthState::EXPOSED}));
}

TEST(HazardTransmissionModelTest, GetsInfectionOutcomesWithFarDistance) {
  absl::MockingBitGen mock_bitgen;
  HazardTransmissionModel transmission_model(
      HazardTransmissionOptions(),
      [](const float, const absl::Time) { return; }, mock_bitgen);

  std::vector<Exposure> exposures{{
      .duration = kLongDuration,
      .distance = kFarDistance,
      .infectivity = 1,
      .symptom_factor = 1,
      .susceptibility = 1,
      .location_transmissibility = 1,
  }};

  EXPECT_CALL(absl::MockBernoulli(), Call(mock_bitgen, testing::_))
      .WillOnce(testing::Return(false));

  EXPECT_THAT(transmission_model.GetInfectionOutcome(MakePointers(exposures)),
              Eq(HealthTransition{.time = absl::UnixEpoch() + kLongDuration,
                                  .health_state = HealthState::SUSCEPTIBLE}));
}

TEST(HazardTransmissionModelTest, GetsHazard) {
  Hazard hazard;
  std::vector<Exposure> exposures{{
      .start_time = absl::UnixEpoch(),
      .duration = kLongDuration,
      .distance = kCloseDistance,
      .infectivity = 1,
      .symptom_factor = 1,
      .susceptibility = 1,
      .location_transmissibility = 1,
  }};
  EXPECT_EQ(0.0,
            hazard.GetHazard(Timestep(absl::UnixEpoch(), absl::Hours(24))));
  hazard.GetTransmissionModel()->GetInfectionOutcome(MakePointers(exposures));
  EXPECT_GT(hazard.GetHazard(
                Timestep(absl::UnixEpoch() + absl::Hours(24), absl::Hours(24))),
            0);
  EXPECT_EQ(0.0, hazard.GetHazard(Timestep(absl::UnixEpoch() + absl::Hours(26),
                                           absl::Hours(24))));
}

}  // namespace
}  // namespace abesim
