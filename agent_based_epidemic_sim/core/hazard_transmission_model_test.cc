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

#include "agent_based_epidemic_sim/core/hazard_transmission_model.h"

#include "absl/time/time.h"
#include "agent_based_epidemic_sim/core/event.h"
#include "agent_based_epidemic_sim/core/visit.h"
#include "gmock/gmock.h"
#include "gtest/gtest.h"

namespace abesim {
namespace {

using testing::Eq;
const ProximityTrace kCloseShortProximityTrace({1});

const ProximityTrace kCloseLongProximityTrace({1, 1, 1, 1, 1, 1, 1, 1, 1, 1,
                                               1, 1, 1, 1, 1, 1, 1, 1, 1, 1});

const ProximityTrace kFarShortProximityTrace({10});

std::vector<const Exposure*> MakePointers(const std::vector<Exposure>& v) {
  std::vector<const Exposure*> result;
  result.reserve(v.size());
  for (const Exposure& e : v) {
    result.push_back(&e);
  }
  return result;
}

TEST(HazardTransmissionModelTest, GetsInfectionOutcomes) {
  HazardTransmissionModel transmission_model(
      {.risk_at_distance_function = [](float proximity) {
        return (proximity <= 1) ? 10 : 0;
      }});

  std::vector<Exposure> exposures{{.duration = absl::Minutes(5),
                                   .proximity_trace = kFarShortProximityTrace,
                                   .infectivity = 1,
                                   .symptom_factor = 1},
                                  {.duration = absl::Minutes(5),
                                   .proximity_trace = kCloseShortProximityTrace,
                                   .infectivity = 1,
                                   .symptom_factor = 1},
                                  {.duration = absl::Minutes(100),
                                   .proximity_trace = kCloseLongProximityTrace,
                                   .infectivity = 1,
                                   .symptom_factor = 1}};

  EXPECT_THAT(
      transmission_model.GetInfectionOutcome(MakePointers(exposures)),
      Eq(HealthTransition{.time = absl::FromUnixSeconds(
                              absl::ToDoubleSeconds(absl::Minutes(100))),
                          .health_state = HealthState::EXPOSED}));

  exposures = {{.duration = absl::Minutes(5),
                .proximity_trace = kFarShortProximityTrace,
                .infectivity = 1,
                .symptom_factor = 1},
               {.duration = absl::Minutes(5),
                .proximity_trace = kCloseShortProximityTrace,
                .infectivity = 1,
                .symptom_factor = 1}};
  EXPECT_THAT(transmission_model.GetInfectionOutcome(MakePointers(exposures)),
              Eq(HealthTransition{.time = absl::FromUnixSeconds(
                                      absl::ToDoubleSeconds(absl::Minutes(5))),
                                  .health_state = HealthState::EXPOSED}));

  exposures = {{.duration = absl::Minutes(5),
                .proximity_trace = kFarShortProximityTrace,
                .infectivity = 1,
                .symptom_factor = 1}};
  EXPECT_THAT(transmission_model.GetInfectionOutcome(MakePointers(exposures)),
              Eq(HealthTransition{.time = absl::FromUnixSeconds(
                                      absl::ToDoubleSeconds(absl::Minutes(5))),
                                  .health_state = HealthState::SUSCEPTIBLE}));
}

TEST(HazardTransmissionModelTest,
     GetsInfectionOutcomesWithDefaultRiskDistance) {
  HazardTransmissionModel transmission_model;

  std::vector<Exposure> exposures{{.duration = absl::Minutes(100),
                                   .proximity_trace = kCloseLongProximityTrace,
                                   .infectivity = 1,
                                   .symptom_factor = 1}};

  EXPECT_THAT(
      transmission_model.GetInfectionOutcome(MakePointers(exposures)),
      Eq(HealthTransition{.time = absl::FromUnixSeconds(
                              absl::ToDoubleSeconds(absl::Minutes(100))),
                          .health_state = HealthState::EXPOSED}));

  exposures = {{.duration = absl::Minutes(100),
                .proximity_trace = kFarShortProximityTrace,
                .infectivity = 1,
                .symptom_factor = 1}};
  EXPECT_THAT(
      transmission_model.GetInfectionOutcome(MakePointers(exposures)),
      Eq(HealthTransition{.time = absl::FromUnixSeconds(
                              absl::ToDoubleSeconds(absl::Minutes(100))),
                          .health_state = HealthState::SUSCEPTIBLE}));
}

}  // namespace
}  // namespace abesim
