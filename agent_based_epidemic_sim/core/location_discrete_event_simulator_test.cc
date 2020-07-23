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

#include "agent_based_epidemic_sim/core/location_discrete_event_simulator.h"

#include "absl/time/time.h"
#include "agent_based_epidemic_sim/core/broker.h"
#include "agent_based_epidemic_sim/core/event.h"
#include "agent_based_epidemic_sim/core/integral_types.h"
#include "agent_based_epidemic_sim/core/observer.h"
#include "agent_based_epidemic_sim/core/pandemic.pb.h"
#include "agent_based_epidemic_sim/core/visit.h"
#include "gmock/gmock.h"
#include "gtest/gtest.h"

namespace abesim {
namespace {

using testing::UnorderedElementsAreArray;

class MockInfectionBroker : public Broker<InfectionOutcome> {
 public:
  MockInfectionBroker() = default;
  MOCK_METHOD(void, Send,
              (absl::Span<const InfectionOutcome> infection_outcome),
              (override));
};

std::vector<InfectionOutcome> InfectionOutcomesFromContacts(
    const absl::Span<const Contact> contacts, const int64 uuid) {
  std::vector<InfectionOutcome> infection_outcomes;
  for (const Contact& contact : contacts) {
    infection_outcomes.push_back(
        {.agent_uuid = uuid,
         .exposure = contact.exposure,
         .exposure_type = InfectionOutcomeProto::CONTACT,
         .source_uuid = contact.other_uuid});
  }
  return infection_outcomes;
}

// TODO: Add test coverage of health states ASYMPTOMATIC, MILD,
// SEVERE.
// TODO: Test that micro_exposure_counts never over-assigns
// durations.
TEST(LocationDiscreteEventSimulatorTest, ContactTracing) {
  const int64 kUuid = 42LL;
  std::vector<Visit> visits{Visit{.location_uuid = 42LL,
                                  .agent_uuid = 0LL,
                                  .start_time = absl::FromUnixSeconds(0LL),
                                  .end_time = absl::FromUnixSeconds(1000LL),
                                  .health_state = HealthState::INFECTIOUS,
                                  .infectivity = 1.0f},
                            Visit{.location_uuid = 42LL,
                                  .agent_uuid = 1LL,
                                  .start_time = absl::FromUnixSeconds(100LL),
                                  .end_time = absl::FromUnixSeconds(500LL),
                                  .health_state = HealthState::SUSCEPTIBLE,
                                  .infectivity = 0.0f},
                            Visit{.location_uuid = 42LL,
                                  .agent_uuid = 2LL,
                                  .start_time = absl::FromUnixSeconds(1000LL),
                                  .end_time = absl::FromUnixSeconds(2000LL),
                                  .health_state = HealthState::EXPOSED,
                                  .infectivity = 0.0f},
                            Visit{.location_uuid = 42LL,
                                  .agent_uuid = 3LL,
                                  .start_time = absl::FromUnixSeconds(400LL),
                                  .end_time = absl::FromUnixSeconds(600LL),
                                  .health_state = HealthState::RECOVERED,
                                  .infectivity = 0.0f}};

  std::vector<Contact> contacts;
  MockInfectionBroker infection_broker;
  {
    std::vector<Contact> expected_contacts = {
        {
            .other_uuid = 1,
            .other_state = HealthState::SUSCEPTIBLE,
            .exposure = {.duration = absl::Seconds(400),
                         .micro_exposure_counts = {1, 1, 1, 1, 1, 1, 0, 0, 0,
                                                   0},
                         .infectivity = 0.0f},
        },
        {
            .other_uuid = 3,
            .other_state = HealthState::RECOVERED,
            .exposure = {.duration = absl::Seconds(200),
                         .micro_exposure_counts = {1, 1, 1, 0, 0, 0, 0, 0, 0,
                                                   0},
                         .infectivity = 0.0f},
        }};
    EXPECT_CALL(infection_broker,
                Send(UnorderedElementsAreArray(InfectionOutcomesFromContacts(
                    expected_contacts, visits[0].agent_uuid))))
        .Times(1);
  }
  {
    std::vector<Contact> expected_contacts = {
        {
            .other_uuid = 0,
            .other_state = HealthState::INFECTIOUS,
            .exposure = {.duration = absl::Seconds(400),
                         .micro_exposure_counts = {1, 1, 1, 1, 1, 1, 0, 0, 0,
                                                   0},
                         .infectivity = 1.0f},
        },
        {
            .other_uuid = 3,
            .other_state = HealthState::RECOVERED,
            .exposure = {.duration = absl::Seconds(100),
                         .micro_exposure_counts = {1, 0, 0, 0, 0, 0, 0, 0, 0,
                                                   0},
                         .infectivity = 0.0f},
        }};
    EXPECT_CALL(infection_broker,
                Send(UnorderedElementsAreArray(InfectionOutcomesFromContacts(
                    expected_contacts, visits[1].agent_uuid))))
        .Times(1);
  }
  {
    std::vector<Contact> expected_contacts = {};
    EXPECT_CALL(infection_broker,
                Send(UnorderedElementsAreArray(InfectionOutcomesFromContacts(
                    expected_contacts, visits[2].agent_uuid))))
        .Times(1);
  }
  {
    std::vector<Contact> expected_contacts = {
        {
            .other_uuid = 0,
            .other_state = HealthState::INFECTIOUS,
            .exposure =
                {
                    .duration = absl::Seconds(200),
                    .micro_exposure_counts = {1, 1, 1, 0, 0, 0, 0, 0, 0, 0},
                    .infectivity = 1.0f,
                },
        },
        {
            .other_uuid = 1,
            .other_state = HealthState::SUSCEPTIBLE,
            .exposure = {.duration = absl::Seconds(100),
                         .micro_exposure_counts = {1, 0, 0, 0, 0, 0, 0, 0, 0,
                                                   0},
                         .infectivity = 0.0f},
        }};
    EXPECT_CALL(infection_broker,
                Send(UnorderedElementsAreArray(InfectionOutcomesFromContacts(
                    expected_contacts, visits[3].agent_uuid))))
        .Times(1);
  }

  LocationDiscreteEventSimulator location(kUuid);
  location.ProcessVisits(visits, &infection_broker);
}

TEST(LocationDiscreteEventSimulatorTest, ProcessVisitsRejectsWrongUuid) {
  auto infection_broker = absl::make_unique<MockInfectionBroker>();
  const int64 kUuid = 42LL;
  std::vector<Visit> visits{Visit{.location_uuid = 314LL,
                                  .agent_uuid = 0LL,
                                  .start_time = absl::FromUnixSeconds(0LL),
                                  .end_time = absl::FromUnixSeconds(86400LL),
                                  .health_state = HealthState::INFECTIOUS}};
  ASSERT_DEBUG_DEATH(LocationDiscreteEventSimulator(kUuid).ProcessVisits(
                         visits, infection_broker.get()),
                     "");
}

TEST(LocationDiscreteEventSimulatorTest,
     ProcessVisitsRejectsStartTimeNotBeforeEndTime) {
  auto infection_broker = absl::make_unique<MockInfectionBroker>();
  const int64 kUuid = 42LL;
  std::vector<Visit> visits{Visit{.location_uuid = kUuid,
                                  .agent_uuid = 0LL,
                                  .start_time = absl::FromUnixSeconds(0LL),
                                  .end_time = absl::FromUnixSeconds(0LL),
                                  .health_state = HealthState::INFECTIOUS}};
  ASSERT_DEBUG_DEATH(LocationDiscreteEventSimulator(kUuid).ProcessVisits(
                         visits, infection_broker.get()),
                     "");
}

}  // namespace
}  // namespace abesim
