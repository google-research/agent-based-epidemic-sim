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

#include "agent_based_epidemic_sim/core/seir_agent.h"

#include "absl/time/time.h"
#include "agent_based_epidemic_sim/core/broker.h"
#include "agent_based_epidemic_sim/core/integral_types.h"
#include "agent_based_epidemic_sim/core/public_policy.h"
#include "agent_based_epidemic_sim/core/timestep.h"
#include "agent_based_epidemic_sim/core/transition_model.h"
#include "agent_based_epidemic_sim/core/visit.h"
#include "agent_based_epidemic_sim/core/visit_generator.h"
#include "gmock/gmock.h"
#include "gtest/gtest.h"

namespace pandemic {
namespace {

using testing::_;
using testing::Eq;
using testing::NotNull;
using testing::Return;
using testing::SetArgPointee;

class MockTransitionModel : public TransitionModel {
 public:
  explicit MockTransitionModel() = default;
  MOCK_METHOD(HealthTransition, GetNextHealthTransition,
              (const HealthTransition& latest_transition), (override));
};

class MockTransmissionModel : public TransmissionModel {
 public:
  MockTransmissionModel() = default;
  MOCK_METHOD(HealthTransition, GetInfectionOutcome,
              (absl::Span<const Exposure* const> exposures), (override));
};

class MockVisitGenerator : public VisitGenerator {
 public:
  explicit MockVisitGenerator() = default;
  MOCK_METHOD(void, GenerateVisits,
              (const Timestep& timestep, const PublicPolicy* policy,
               HealthState::State current_health_state,
               const ContactSummary& contact_summary,
               std::vector<Visit>* visits),
              (override));
};

template <typename T>
class MockBroker : public Broker<T> {
 public:
  explicit MockBroker() = default;
  MOCK_METHOD(void, Send, (absl::Span<const T> visits), (override));
};

class MockPublicPolicy : public PublicPolicy {
 public:
  MOCK_METHOD(VisitAdjustment, GetVisitAdjustment,
              (const Timestep& timestep, HealthState::State health_state,
               const ContactSummary& contact_summary, int64 location_uuid),
              (const, override));
  MOCK_METHOD(TestPolicy, GetTestPolicy,
              (const ContactSummary& contact_summary,
               const TestResult& previous_test_result),
              (const, override));
  MOCK_METHOD(ContactTracingPolicy, GetContactTracingPolicy,
              (absl::Span<const ContactReport> received_contact_reports,
               const TestResult& test_result),
              (const, override));
  MOCK_METHOD(absl::Duration, ContactRetentionDuration, (), (const, override));
};

InfectionOutcome InfectionOutcomeFromContact(const int64 agent_uuid,
                                             const Contact& contact) {
  return {
      .agent_uuid = agent_uuid,
      .exposure = contact.exposure,
      .exposure_type = InfectionOutcomeProto::CONTACT,
      .source_uuid = contact.other_uuid,
  };
}

TEST(SEIRAgentTest, ComputesVisits) {
  auto transition_model = absl::make_unique<MockTransitionModel>();
  auto visit_generator = absl::make_unique<MockVisitGenerator>();
  auto visit_broker = absl::make_unique<MockBroker<Visit>>();
  MockTransmissionModel transmission_model;
  auto public_policy = NewNoOpPolicy();
  const Timestep timestep(absl::UnixEpoch(), absl::Hours(24));
  EXPECT_CALL(*transition_model, GetNextHealthTransition(Eq(HealthTransition{
                                     .time = absl::FromUnixSeconds(-43200LL),
                                     .health_state = HealthState::EXPOSED})))
      .WillOnce(
          Return(HealthTransition{.time = absl::FromUnixSeconds(43200LL),
                                  .health_state = HealthState::INFECTIOUS}));
  EXPECT_CALL(*transition_model, GetNextHealthTransition(Eq(HealthTransition{
                                     .time = absl::FromUnixSeconds(43200LL),
                                     .health_state = HealthState::INFECTIOUS})))
      .Times(1);
  const int64 kUuid = 42LL;
  std::vector<Visit> visits{Visit{.location_uuid = 0LL,
                                  .start_time = absl::FromUnixSeconds(0LL),
                                  .end_time = absl::FromUnixSeconds(28800LL)},
                            Visit{.location_uuid = 1LL,
                                  .start_time = absl::FromUnixSeconds(28800LL),
                                  .end_time = absl::FromUnixSeconds(57600LL)},
                            Visit{.location_uuid = 0LL,
                                  .start_time = absl::FromUnixSeconds(57600LL),
                                  .end_time = absl::FromUnixSeconds(86400LL)}};
  EXPECT_CALL(*visit_generator,
              GenerateVisits(timestep, public_policy.get(),
                             HealthState::INFECTIOUS, _, NotNull()))
      .WillOnce(SetArgPointee<4>(visits));
  std::vector<Visit> expected_visits{
      Visit{.location_uuid = 0LL,
            .agent_uuid = kUuid,
            .start_time = absl::FromUnixSeconds(0LL),
            .end_time = absl::FromUnixSeconds(28800LL),
            .health_state = HealthState::EXPOSED},
      Visit{.location_uuid = 1LL,
            .agent_uuid = kUuid,
            .start_time = absl::FromUnixSeconds(28800LL),
            .end_time = absl::FromUnixSeconds(43200LL),
            .health_state = HealthState::EXPOSED},
      Visit{.location_uuid = 0LL,
            .agent_uuid = kUuid,
            .start_time = absl::FromUnixSeconds(57600LL),
            .end_time = absl::FromUnixSeconds(86400LL),
            .health_state = HealthState::INFECTIOUS},
      Visit{.location_uuid = 1LL,
            .agent_uuid = kUuid,
            .start_time = absl::FromUnixSeconds(43200LL),
            .end_time = absl::FromUnixSeconds(57600LL),
            .health_state = HealthState::INFECTIOUS}};
  EXPECT_CALL(*visit_broker, Send(Eq(expected_visits)));
  auto agent =
      SEIRAgent::Create(kUuid,
                        {.time = absl::FromUnixSeconds(-43200LL),
                         .health_state = HealthState::EXPOSED},
                        &transmission_model, std::move(transition_model),
                        std::move(visit_generator), public_policy.get());
  agent->ProcessInfectionOutcomes(timestep, {});
  agent->ComputeVisits(timestep, visit_broker.get());
}

TEST(SEIRAgentTest, InitializesSusceptibleState) {
  auto transition_model = absl::make_unique<MockTransitionModel>();
  auto visit_generator = absl::make_unique<MockVisitGenerator>();
  auto visit_broker = absl::make_unique<MockBroker<Visit>>();
  MockTransmissionModel transmission_model;
  auto public_policy = NewNoOpPolicy();
  const Timestep timestep(absl::UnixEpoch(), absl::Hours(24));
  EXPECT_CALL(*transition_model, GetNextHealthTransition).Times(0);
  const int64 kUuid = 42LL;
  std::vector<Visit> visits{Visit{.location_uuid = 0LL,
                                  .start_time = absl::FromUnixSeconds(0LL),
                                  .end_time = absl::FromUnixSeconds(86400LL)}};
  EXPECT_CALL(*visit_generator,
              GenerateVisits(timestep, public_policy.get(),
                             HealthState::SUSCEPTIBLE, _, NotNull()))
      .WillOnce(SetArgPointee<4>(visits));
  std::vector<Visit> expected_visits{
      Visit{.location_uuid = 0LL,
            .agent_uuid = kUuid,
            .start_time = absl::FromUnixSeconds(0LL),
            .end_time = absl::FromUnixSeconds(86400LL),
            .health_state = HealthState::SUSCEPTIBLE}};
  EXPECT_CALL(*visit_broker, Send(Eq(expected_visits)));
  auto agent = SEIRAgent::CreateSusceptible(
      kUuid, &transmission_model, std::move(transition_model),
      std::move(visit_generator), public_policy.get());
  agent->ComputeVisits(timestep, visit_broker.get());
}

TEST(SEIRAgentTest, InitializesNonSusceptibleState) {
  auto transition_model = absl::make_unique<MockTransitionModel>();
  auto visit_generator = absl::make_unique<MockVisitGenerator>();
  auto visit_broker = absl::make_unique<MockBroker<Visit>>();
  MockTransmissionModel transmission_model;
  auto public_policy = NewNoOpPolicy();
  const Timestep timestep(absl::UnixEpoch(), absl::Hours(24));
  EXPECT_CALL(*transition_model, GetNextHealthTransition(Eq(HealthTransition{
                                     .time = absl::FromUnixSeconds(-1LL),
                                     .health_state = HealthState::EXPOSED})))
      .WillOnce(
          Return(HealthTransition{.time = absl::FromUnixSeconds(86400LL),
                                  .health_state = HealthState::INFECTIOUS}));
  const int64 kUuid = 42LL;
  std::vector<Visit> visits{Visit{.location_uuid = 0LL,
                                  .start_time = absl::FromUnixSeconds(0LL),
                                  .end_time = absl::FromUnixSeconds(86400LL)}};
  EXPECT_CALL(*visit_generator,
              GenerateVisits(timestep, public_policy.get(),
                             HealthState::EXPOSED, _, NotNull()))
      .WillOnce(SetArgPointee<4>(visits));
  std::vector<Visit> expected_visits{
      Visit{.location_uuid = 0LL,
            .agent_uuid = kUuid,
            .start_time = absl::FromUnixSeconds(0LL),
            .end_time = absl::FromUnixSeconds(86400LL),
            .health_state = HealthState::EXPOSED}};
  EXPECT_CALL(*visit_broker, Send(Eq(expected_visits)));
  auto agent =
      SEIRAgent::Create(kUuid,
                        {.time = absl::FromUnixSeconds(-1LL),
                         .health_state = HealthState::EXPOSED},
                        &transmission_model, std::move(transition_model),
                        std::move(visit_generator), public_policy.get());
  agent->ProcessInfectionOutcomes(timestep, {});
  agent->ComputeVisits(timestep, visit_broker.get());
}

TEST(SEIRAgentTest, RespectsTimestepBasedDwellTimeAndFiltersZeroIntervals) {
  auto transition_model = absl::make_unique<MockTransitionModel>();
  auto visit_generator = absl::make_unique<MockVisitGenerator>();
  auto visit_broker = absl::make_unique<MockBroker<Visit>>();
  MockTransmissionModel transmission_model;
  auto public_policy = NewNoOpPolicy();
  const Timestep timestep(absl::UnixEpoch(), absl::Hours(24));
  EXPECT_CALL(*transition_model, GetNextHealthTransition(Eq(HealthTransition{
                                     .time = absl::FromUnixSeconds(-1LL),
                                     .health_state = HealthState::EXPOSED})))
      .WillOnce(
          Return(HealthTransition{.time = absl::FromUnixSeconds(-1LL),
                                  .health_state = HealthState::INFECTIOUS}));
  // Transition is recorded with a forward-adjusted transition time.
  EXPECT_CALL(*transition_model,
              GetNextHealthTransition(Eq(
                  HealthTransition{.time = absl::FromUnixSeconds(86400LL - 1LL),
                                   .health_state = HealthState::INFECTIOUS})))
      .WillOnce(
          Return(HealthTransition{.time = absl::FromUnixSeconds(2LL * 86400LL),
                                  .health_state = HealthState::RECOVERED}));
  const int64 kUuid = 42LL;
  std::vector<Visit> visits{Visit{.location_uuid = 0LL,
                                  .start_time = absl::FromUnixSeconds(0LL),
                                  .end_time = absl::FromUnixSeconds(86400LL)}};
  EXPECT_CALL(*visit_generator,
              GenerateVisits(timestep, public_policy.get(),
                             HealthState::INFECTIOUS, _, NotNull()))
      .WillOnce(SetArgPointee<4>(visits));
  std::vector<Visit> expected_visits{
      Visit{.location_uuid = 0LL,
            .agent_uuid = kUuid,
            .start_time = absl::FromUnixSeconds(0LL),
            .end_time = absl::FromUnixSeconds(86400LL - 1LL),
            .health_state = HealthState::EXPOSED},
      Visit{.location_uuid = 0LL,
            .agent_uuid = kUuid,
            .start_time = absl::FromUnixSeconds(86400LL - 1LL),
            .end_time = absl::FromUnixSeconds(86400LL),
            .health_state = HealthState::INFECTIOUS},
  };
  EXPECT_CALL(*visit_broker, Send(Eq(expected_visits)));
  auto agent =
      SEIRAgent::Create(kUuid,
                        {.time = absl::FromUnixSeconds(-1LL),
                         .health_state = HealthState::EXPOSED},
                        &transmission_model, std::move(transition_model),
                        std::move(visit_generator), public_policy.get());
  agent->ProcessInfectionOutcomes(timestep, {});
  agent->ComputeVisits(timestep, visit_broker.get());
}

TEST(SEIRAgentTest, ProcessesInfectionOutcomesIgnoresIfAlreadyExposed) {
  auto transition_model = absl::make_unique<MockTransitionModel>();
  EXPECT_CALL(*transition_model, GetNextHealthTransition(Eq(HealthTransition{
                                     .time = absl::FromUnixSeconds(-1LL),
                                     .health_state = HealthState::EXPOSED})))
      .WillOnce(
          Return(HealthTransition{.time = absl::FromUnixSeconds(86400LL),
                                  .health_state = HealthState::INFECTIOUS}));
  auto visit_generator = absl::make_unique<MockVisitGenerator>();
  MockTransmissionModel transmission_model;
  EXPECT_CALL(transmission_model, GetInfectionOutcome(_))
      .Times(1)
      .WillOnce(Return(HealthTransition{.time = absl::FromUnixSeconds(-1LL),
                                        .health_state = HealthState::EXPOSED}));
  auto public_policy = NewNoOpPolicy();
  const int64 kUuid = 42LL;

  auto agent = SEIRAgent::CreateSusceptible(
      kUuid, &transmission_model, std::move(transition_model),
      std::move(visit_generator), public_policy.get());

  const Timestep timestep(absl::UnixEpoch(), absl::Hours(24));
  {
    std::vector<InfectionOutcome> infection_outcomes{
        InfectionOutcome{.agent_uuid = kUuid,
                         .exposure = {.start_time = absl::FromUnixSeconds(-1LL),
                                      .infectivity = 1.0f},
                         .exposure_type = InfectionOutcomeProto::CONTACT,
                         .source_uuid = 2LL}};
    agent->ProcessInfectionOutcomes(timestep, infection_outcomes);
    EXPECT_EQ(agent->NextHealthTransition().time,
              absl::FromUnixSeconds(86400LL));
  }
  {
    std::vector<InfectionOutcome> infection_outcomes{
        InfectionOutcome{.agent_uuid = kUuid,
                         .exposure = {.start_time = absl::FromUnixSeconds(5LL),
                                      .infectivity = 1.0f},
                         .exposure_type = InfectionOutcomeProto::CONTACT,
                         .source_uuid = 3LL}};
    // A new call with an infection outcome with a different time has no
    // effect, only the first exposure matters.
    agent->ProcessInfectionOutcomes(timestep, infection_outcomes);
    EXPECT_EQ(agent->NextHealthTransition().time,
              absl::FromUnixSeconds(86400LL));
  }
}

TEST(SEIRAgentTest, ProcessesInfectionOutcomesRemainsSusceptible) {
  auto transition_model = absl::make_unique<MockTransitionModel>();
  EXPECT_CALL(*transition_model, GetNextHealthTransition).Times(0);
  auto visit_generator = absl::make_unique<MockVisitGenerator>();
  MockTransmissionModel transmission_model;
  auto public_policy = NewNoOpPolicy();
  const int64 kUuid = 42LL;

  EXPECT_CALL(transmission_model, GetInfectionOutcome(_))
      .Times(1)
      .WillOnce(
          Return(HealthTransition{.health_state = HealthState::SUSCEPTIBLE}));
  auto agent = SEIRAgent::CreateSusceptible(
      kUuid, &transmission_model, std::move(transition_model),
      std::move(visit_generator), public_policy.get());

  const Timestep timestep(absl::UnixEpoch(), absl::Hours(24));
  std::vector<InfectionOutcome> infection_outcomes{
      InfectionOutcome{.agent_uuid = kUuid,
                       .exposure = {.start_time = absl::FromUnixSeconds(-1LL),
                                    .infectivity = 1.0f},
                       .exposure_type = InfectionOutcomeProto::CONTACT,
                       .source_uuid = 2LL}};
  agent->ProcessInfectionOutcomes(timestep, infection_outcomes);
  EXPECT_THAT(agent->NextHealthTransition(),
              Eq(HealthTransition{.time = absl::InfiniteFuture(),
                                  .health_state = HealthState::SUSCEPTIBLE}));
}

TEST(SEIRAgentTest, ProcessesInfectionOutcomesMultipleExposuresSameContact) {
  auto transition_model = absl::make_unique<MockTransitionModel>();
  EXPECT_CALL(*transition_model, GetNextHealthTransition).Times(0);
  auto visit_generator = absl::make_unique<MockVisitGenerator>();
  MockTransmissionModel transmission_model;
  auto public_policy = NewNoOpPolicy();
  const int64 kUuid = 42LL;

  EXPECT_CALL(transmission_model, GetInfectionOutcome(_)).Times(1);
  auto agent = SEIRAgent::CreateSusceptible(
      kUuid, &transmission_model, std::move(transition_model),
      std::move(visit_generator), public_policy.get());

  const Timestep timestep(absl::UnixEpoch(), absl::Hours(24));
  std::vector<InfectionOutcome> infection_outcomes{
      InfectionOutcome{.agent_uuid = kUuid,
                       .exposure = {.start_time = absl::FromUnixSeconds(-2LL),
                                    .infectivity = 1.0f},
                       .exposure_type = InfectionOutcomeProto::CONTACT,
                       .source_uuid = 2LL},
      InfectionOutcome{.agent_uuid = kUuid,
                       .exposure = {.start_time = absl::FromUnixSeconds(-1LL),
                                    .infectivity = 1.0f},
                       .exposure_type = InfectionOutcomeProto::CONTACT,
                       .source_uuid = 2LL}};
  agent->ProcessInfectionOutcomes(timestep, infection_outcomes);
}

TEST(SEIRAgentTest, ProcessInfectionOutcomesRejectsWrongUuid) {
  auto transition_model = absl::make_unique<MockTransitionModel>();
  auto visit_generator = absl::make_unique<MockVisitGenerator>();
  MockTransmissionModel transmission_model;
  auto public_policy = NewNoOpPolicy();
  const int64 kUuid = 42LL;
  std::vector<InfectionOutcome> infection_outcomes{InfectionOutcome{
      .agent_uuid = kUuid + 1,
  }};
  auto agent = SEIRAgent::CreateSusceptible(
      kUuid, &transmission_model, std::move(transition_model),
      std::move(visit_generator), public_policy.get());
  const Timestep timestep(absl::UnixEpoch(), absl::Hours(24));
  ASSERT_DEBUG_DEATH(
      agent->ProcessInfectionOutcomes(timestep, infection_outcomes), "");
}

TEST(SEIRAgentTest, ProcessInfectionOutcomesReturnsNoOpIfNonePresent) {
  auto transition_model = absl::make_unique<MockTransitionModel>();
  auto visit_generator = absl::make_unique<MockVisitGenerator>();
  MockTransmissionModel transmission_model;
  auto public_policy = NewNoOpPolicy();
  EXPECT_CALL(*transition_model, GetNextHealthTransition).Times(0);
  const int64 kUuid = 42LL;
  auto agent = SEIRAgent::CreateSusceptible(
      kUuid, &transmission_model, std::move(transition_model),
      std::move(visit_generator), public_policy.get());
  const Timestep timestep(absl::UnixEpoch(), absl::Hours(24));
  agent->ProcessInfectionOutcomes(timestep, {});
}

TEST(SEIRAgentTest, NoOpUpdateContactReports) {
  auto transition_model = absl::make_unique<MockTransitionModel>();
  auto visit_generator = absl::make_unique<MockVisitGenerator>();
  MockTransmissionModel transmission_model;
  auto public_policy = absl::make_unique<MockPublicPolicy>();
  const int64 kUuid = 42LL;
  auto agent = SEIRAgent::CreateSusceptible(
      kUuid, &transmission_model, std::move(transition_model),
      std::move(visit_generator), public_policy.get());
  const TestResult init_test_result{.time_requested = absl::InfiniteFuture(),
                                    .time_received = absl::InfiniteFuture(),
                                    .needs_retry = false,
                                    .probability = 0};
  std::vector<ContactReport> contact_reports;
  ContactSummary contact_summary{.retention_horizon = absl::InfiniteFuture(),
                                 .latest_contact_time = absl::InfinitePast()};
  EXPECT_CALL(*public_policy,
              GetTestPolicy(Eq(contact_summary), Eq(init_test_result)))
      .WillOnce(Return(PublicPolicy::TestPolicy{.should_test = false}));
  EXPECT_CALL(*public_policy, GetContactTracingPolicy(Eq(contact_reports),
                                                      Eq(init_test_result)))
      .WillOnce(Return(PublicPolicy::ContactTracingPolicy{}));
  auto contact_report_broker = absl::make_unique<MockBroker<ContactReport>>();
  EXPECT_CALL(*contact_report_broker, Send(testing::_)).Times(0);
  agent->UpdateContactReports(contact_reports, contact_report_broker.get());
}

TEST(SEIRAgentTest, PositiveTest) {
  auto transition_model = absl::make_unique<MockTransitionModel>();
  auto visit_generator = absl::make_unique<MockVisitGenerator>();
  MockTransmissionModel transmission_model;
  auto public_policy = absl::make_unique<MockPublicPolicy>();
  const int64 kUuid = 42LL;
  auto agent =
      SEIRAgent::Create(kUuid,
                        {.time = absl::FromUnixSeconds(-1LL),
                         .health_state = HealthState::INFECTIOUS},
                        &transmission_model, std::move(transition_model),
                        std::move(visit_generator), public_policy.get());
  const TestResult init_test_result{.time_requested = absl::InfiniteFuture(),
                                    .time_received = absl::InfiniteFuture(),
                                    .needs_retry = false,
                                    .probability = 0};
  std::vector<ContactReport> contact_reports;
  std::vector<Contact> contacts{
      {.other_uuid = 314LL,
       .exposure = {.start_time = absl::FromUnixSeconds(0LL),
                    .duration = absl::Hours(1LL)}}};
  const TestResult expected_test_result{
      .time_requested = absl::FromUnixSeconds(0LL),
      .time_received = absl::FromUnixSeconds(129600LL),
      .needs_retry = false,
      .probability = 1.0f};
  EXPECT_CALL(*public_policy, GetTestPolicy(_, Eq(init_test_result)))
      .WillOnce(Return(
          PublicPolicy::TestPolicy{.should_test = true,
                                   .time_requested = absl::FromUnixSeconds(0LL),
                                   .latency = absl::Hours(36)}));
  EXPECT_CALL(*public_policy, GetContactTracingPolicy(Eq(contact_reports),
                                                      Eq(expected_test_result)))
      .WillOnce(Return(
          PublicPolicy::ContactTracingPolicy{.send_positive_test = true}));
  auto contact_report_broker = absl::make_unique<MockBroker<ContactReport>>();
  const std::vector<ContactReport> expected_contact_reports{
      {.from_agent_uuid = kUuid,
       .to_agent_uuid = 314LL,
       .test_result = expected_test_result}};
  EXPECT_CALL(*contact_report_broker, Send(Eq(expected_contact_reports)))
      .Times(1);
  const Timestep timestep(absl::UnixEpoch(), absl::Hours(24));
  agent->ProcessInfectionOutcomes(
      timestep, {InfectionOutcomeFromContact(kUuid, *contacts.begin())});
  agent->UpdateContactReports(contact_reports, contact_report_broker.get());
}

TEST(SEIRAgentTest, NegativeTestResult) {
  auto transition_model = absl::make_unique<MockTransitionModel>();
  auto visit_generator = absl::make_unique<MockVisitGenerator>();
  MockTransmissionModel transmission_model;
  auto public_policy = absl::make_unique<MockPublicPolicy>();
  const int64 kUuid = 42LL;
  auto agent = SEIRAgent::CreateSusceptible(
      kUuid, &transmission_model, std::move(transition_model),
      std::move(visit_generator), public_policy.get());
  const TestResult init_test_result{.time_requested = absl::InfiniteFuture(),
                                    .time_received = absl::InfiniteFuture(),
                                    .needs_retry = false,
                                    .probability = 0};

  const TestResult contact_test_result{
      .time_requested = absl::FromUnixSeconds(0LL),
      .time_received = absl::FromUnixSeconds(129600LL),
      .needs_retry = false,
      .probability = 1.0f};
  std::vector<ContactReport> contact_reports{
      {.from_agent_uuid = 314LL,
       .to_agent_uuid = kUuid,
       .test_result = contact_test_result}};
  std::vector<Contact> contacts{
      {.other_uuid = 314LL,
       .exposure = {.start_time = absl::FromUnixSeconds(0LL),
                    .duration = absl::Hours(1LL)}}};
  ContactSummary contact_summary{
      .retention_horizon = absl::FromUnixSeconds(0LL),
      .latest_contact_time = absl::FromUnixSeconds(3600LL)};
  EXPECT_CALL(*public_policy,
              GetTestPolicy(Eq(contact_summary), Eq(init_test_result)))
      .WillOnce(Return(
          PublicPolicy::TestPolicy{.should_test = true,
                                   .time_requested = absl::FromUnixSeconds(0LL),
                                   .latency = absl::Hours(36)}));
  const TestResult expected_test_result{
      .time_requested = absl::FromUnixSeconds(0LL),
      .time_received = absl::FromUnixSeconds(129600LL),
      .needs_retry = false,
      .probability = 0.0f};
  EXPECT_CALL(*public_policy, GetContactTracingPolicy(Eq(contact_reports),
                                                      Eq(expected_test_result)))
      .WillOnce(Return(
          PublicPolicy::ContactTracingPolicy{.send_positive_test = true}));
  auto contact_report_broker = absl::make_unique<MockBroker<ContactReport>>();
  EXPECT_CALL(*contact_report_broker, Send(testing::_)).Times(0);
  const Timestep timestep(absl::UnixEpoch(), absl::Hours(24));
  agent->ProcessInfectionOutcomes(
      timestep, {InfectionOutcomeFromContact(kUuid, *contacts.begin())});
  agent->UpdateContactReports(contact_reports, contact_report_broker.get());
}

TEST(SEIRAgentTest, TestTooEarlyRetries) {
  auto transition_model = absl::make_unique<MockTransitionModel>();
  auto raw_transition_model = transition_model.get();
  auto visit_generator = absl::make_unique<MockVisitGenerator>();
  MockTransmissionModel transmission_model;
  auto public_policy = absl::make_unique<MockPublicPolicy>();
  auto contact_report_broker = absl::make_unique<MockBroker<ContactReport>>();
  const int64 kUuid = 42LL;
  auto agent =
      SEIRAgent::Create(kUuid,
                        {.time = absl::FromUnixSeconds(0LL),
                         .health_state = HealthState::EXPOSED},
                        &transmission_model, std::move(transition_model),
                        std::move(visit_generator), public_policy.get());
  const TestResult expected_test_result_1{
      .time_requested = absl::FromUnixSeconds(129600LL),
      .time_received = absl::InfiniteFuture(),
      .needs_retry = true,
      .probability = 0.0f};
  EXPECT_CALL(*public_policy, ContactRetentionDuration)
      .WillRepeatedly(Return(absl::Hours(336LL)));
  std::vector<Contact> contacts{
      {.other_uuid = 314LL,
       .exposure = {.start_time = absl::FromUnixSeconds(0LL),
                    .duration = absl::Hours(1LL)}}};
  {
    const TestResult test_result{.time_requested = absl::InfiniteFuture(),
                                 .time_received = absl::InfiniteFuture(),
                                 .needs_retry = false,
                                 .probability = 0};
    const TestResult contact_test_result{
        .time_requested = absl::FromUnixSeconds(0LL),
        .time_received = absl::FromUnixSeconds(129600LL),
        .needs_retry = false,
        .probability = 1.0f};
    std::vector<ContactReport> contact_reports{
        {.from_agent_uuid = 314LL,
         .to_agent_uuid = kUuid,
         .test_result = contact_test_result}};
    EXPECT_CALL(*raw_transition_model,
                GetNextHealthTransition(
                    Eq(HealthTransition{.time = absl::FromUnixSeconds(0LL),
                                        .health_state = HealthState::EXPOSED})))
        .WillOnce(
            Return(HealthTransition{.time = absl::FromUnixSeconds(43200LL),
                                    .health_state = HealthState::INFECTIOUS}));
    ContactSummary contact_summary{
        .retention_horizon = absl::UnixEpoch() - absl::Hours(336LL),
        .latest_contact_time = absl::FromUnixSeconds(3600LL)};
    EXPECT_CALL(*public_policy,
                GetTestPolicy(Eq(contact_summary), Eq(test_result)))
        .WillOnce(Return(PublicPolicy::TestPolicy{
            .should_test = true,
            .time_requested = absl::FromUnixSeconds(129600LL),
            .latency = absl::Hours(36)}));
    EXPECT_CALL(*public_policy,
                GetContactTracingPolicy(Eq(contact_reports),
                                        Eq(expected_test_result_1)))
        .WillOnce(Return(
            PublicPolicy::ContactTracingPolicy{.send_positive_test = true}));
    EXPECT_CALL(*contact_report_broker, Send).Times(0);
    const Timestep timestep(absl::UnixEpoch(), absl::Hours(24));
    agent->ProcessInfectionOutcomes(
        timestep, {InfectionOutcomeFromContact(kUuid, *contacts.begin())});
    agent->UpdateContactReports(contact_reports, contact_report_broker.get());
  }
  {
    const TestResult expected_test_result_2{
        .time_requested = absl::FromUnixSeconds(129600LL),
        .time_received = absl::FromUnixSeconds(259200LL),
        .needs_retry = false,
        .probability = 1.0f};
    EXPECT_CALL(*raw_transition_model,
                GetNextHealthTransition(Eq(
                    HealthTransition{.time = absl::FromUnixSeconds(86400LL),
                                     .health_state = HealthState::INFECTIOUS})))
        .WillOnce(
            Return(HealthTransition{.time = absl::FromUnixSeconds(604800LL),
                                    .health_state = HealthState::RECOVERED}));
    std::vector<ContactReport> contact_reports;
    ContactSummary contact_summary{
        .retention_horizon = absl::UnixEpoch() - absl::Hours(312LL),
        .latest_contact_time = absl::FromUnixSeconds(3600LL)};
    EXPECT_CALL(*public_policy,
                GetTestPolicy(Eq(contact_summary), Eq(expected_test_result_1)))
        .WillOnce(Return(PublicPolicy::TestPolicy{
            .should_test = true,
            .time_requested = absl::FromUnixSeconds(129600LL),
            .latency = absl::Hours(36)}));
    EXPECT_CALL(*public_policy,
                GetContactTracingPolicy(Eq(contact_reports),
                                        Eq(expected_test_result_2)))
        .WillOnce(Return(
            PublicPolicy::ContactTracingPolicy{.send_positive_test = true}));
    const std::vector<ContactReport> expected_contact_reports{
        {.from_agent_uuid = kUuid,
         .to_agent_uuid = 314LL,
         .test_result = expected_test_result_2}};
    EXPECT_CALL(*contact_report_broker, Send(Eq(expected_contact_reports)))
        .Times(1);
    const Timestep timestep(absl::FromUnixSeconds(86400LL), absl::Hours(24));
    agent->ProcessInfectionOutcomes(timestep, {});
    agent->UpdateContactReports(contact_reports, contact_report_broker.get());
  }
}

TEST(SEIRAgentTest, UpdatesAndPrunesContacts) {
  auto transition_model = absl::make_unique<MockTransitionModel>();
  auto visit_generator = absl::make_unique<MockVisitGenerator>();
  MockTransmissionModel transmission_model;
  auto public_policy = absl::make_unique<MockPublicPolicy>();
  auto contact_report_broker = absl::make_unique<MockBroker<ContactReport>>();
  const int64 kUuid = 42LL;
  auto agent =
      SEIRAgent::Create(kUuid,
                        {.time = absl::FromUnixSeconds(-1LL),
                         .health_state = HealthState::INFECTIOUS},
                        &transmission_model, std::move(transition_model),
                        std::move(visit_generator), public_policy.get());
  EXPECT_CALL(*public_policy, ContactRetentionDuration)
      .WillRepeatedly(Return(absl::Hours(1LL)));
  EXPECT_CALL(*public_policy, GetContactTracingPolicy(_, _))
      .WillRepeatedly(Return(
          PublicPolicy::ContactTracingPolicy{.send_positive_test = true}));
  const TestResult expected_test_result{
      .time_requested = absl::FromUnixSeconds(0LL),
      .time_received = absl::FromUnixSeconds(43200LL),
      .needs_retry = false,
      .probability = 1.0f};
  {
    std::vector<Contact> contacts{
        {.other_uuid = 314LL,
         .exposure = {.start_time = absl::FromUnixSeconds(43201LL),
                      .duration = absl::Hours(1LL)}},
        {.other_uuid = 272LL,
         .exposure = {.start_time = absl::FromUnixSeconds(43200LL),
                      .duration = absl::Hours(1LL)}}};
    const Timestep timestep(absl::UnixEpoch(), absl::Hours(24));
    agent->ProcessInfectionOutcomes(
        timestep, {InfectionOutcomeFromContact(kUuid, *contacts.begin()),
                   InfectionOutcomeFromContact(kUuid, *contacts.rbegin())});
    EXPECT_CALL(*public_policy, GetTestPolicy(_, _))
        .WillOnce(Return(PublicPolicy::TestPolicy{
            .should_test = true,
            .time_requested = absl::FromUnixSeconds(0LL),
            .latency = absl::Hours(12)}));
    const std::vector<ContactReport> expected_contact_reports{
        {.from_agent_uuid = kUuid,
         .to_agent_uuid = 314LL,
         .test_result = expected_test_result},
        {.from_agent_uuid = kUuid,
         .to_agent_uuid = 272LL,
         .test_result = expected_test_result}};
    EXPECT_CALL(*contact_report_broker, Send(Eq(expected_contact_reports)))
        .Times(1);
    agent->UpdateContactReports({}, contact_report_broker.get());
  }
  {
    std::vector<Contact> contacts{
        {.other_uuid = 314LL,
         .exposure = {.start_time = absl::FromUnixSeconds(86400LL),
                      .duration = absl::Hours(1LL)}}};
    const Timestep timestep(absl::FromUnixSeconds(86400LL), absl::Hours(24));
    agent->ProcessInfectionOutcomes(
        timestep, {InfectionOutcomeFromContact(kUuid, *contacts.begin())});
    EXPECT_CALL(*public_policy, GetTestPolicy(_, _))
        .WillOnce(Return(PublicPolicy::TestPolicy{.should_test = false}));
    const std::vector<ContactReport> expected_contact_reports{
        {.from_agent_uuid = kUuid,
         .to_agent_uuid = 314LL,
         .test_result = expected_test_result}};
    EXPECT_CALL(*contact_report_broker, Send(Eq(expected_contact_reports)))
        .Times(1);
    agent->UpdateContactReports({}, contact_report_broker.get());
  }
}

TEST(SEIRAgentTest, ComputesContactSummary) {
  auto transition_model = absl::make_unique<MockTransitionModel>();
  auto visit_generator = absl::make_unique<MockVisitGenerator>();
  MockTransmissionModel transmission_model;
  auto public_policy = absl::make_unique<MockPublicPolicy>();
  auto contact_report_broker = absl::make_unique<MockBroker<ContactReport>>();
  const int64 kUuid = 42LL;
  auto agent = SEIRAgent::CreateSusceptible(
      kUuid, &transmission_model, std::move(transition_model),
      std::move(visit_generator), public_policy.get());
  EXPECT_CALL(*public_policy, ContactRetentionDuration)
      .WillRepeatedly(Return(absl::Hours(1LL)));
  std::vector<Contact> contacts{
      {.other_uuid = 314LL,
       .exposure = {.start_time = absl::FromUnixSeconds(42199LL),
                    .duration = absl::Hours(1LL)}},
      {.other_uuid = 272LL,
       .exposure = {.start_time = absl::FromUnixSeconds(43200LL),
                    .duration = absl::Hours(11LL)}}};
  const std::vector<ContactReport> contact_reports{
      {.from_agent_uuid = 314LL, .to_agent_uuid = kUuid},
      {.from_agent_uuid = 272LL, .to_agent_uuid = kUuid}};
  {
    const Timestep timestep(absl::FromUnixSeconds(0LL), absl::Hours(24));
    agent->ProcessInfectionOutcomes(
        timestep, {InfectionOutcomeFromContact(kUuid, *contacts.begin()),
                   InfectionOutcomeFromContact(kUuid, *contacts.rbegin())});
    agent->UpdateContactReports(contact_reports, contact_report_broker.get());
    EXPECT_THAT(agent->GetContactSummary(),
                Eq(ContactSummary{
                    .retention_horizon = absl::FromUnixSeconds(-3600LL),
                    .latest_contact_time = absl::FromUnixSeconds(82800LL)}));
  }
  {
    contacts = {{.other_uuid = 314LL,
                 .exposure = {.start_time = absl::FromUnixSeconds(86400LL),
                              .duration = absl::Hours(1LL)}}};
    const Timestep timestep(absl::FromUnixSeconds(86400LL), absl::Hours(24));
    agent->ProcessInfectionOutcomes(
        timestep, {InfectionOutcomeFromContact(kUuid, *contacts.begin())});
    agent->UpdateContactReports(contact_reports, contact_report_broker.get());
    EXPECT_THAT(agent->GetContactSummary(),
                Eq(ContactSummary{
                    .retention_horizon = absl::FromUnixSeconds(82800LL),
                    .latest_contact_time = absl::FromUnixSeconds(90000LL)}));
  }
  {
    contacts = {{.other_uuid = 314LL,
                 .exposure = {.start_time = absl::FromUnixSeconds(169200LL),
                              .duration = absl::Minutes(30)}}};
    const Timestep timestep(absl::FromUnixSeconds(172800LL), absl::Hours(1));
    agent->ProcessInfectionOutcomes(
        timestep, {InfectionOutcomeFromContact(kUuid, *contacts.begin())});
    agent->UpdateContactReports(contact_reports, contact_report_broker.get());
    EXPECT_THAT(agent->GetContactSummary(),
                Eq(ContactSummary{
                    .retention_horizon = absl::FromUnixSeconds(169200LL),
                    .latest_contact_time = absl::FromUnixSeconds(171000LL)}));
  }
  {
    const Timestep timestep(absl::FromUnixSeconds(176400LL), absl::Hours(1));
    agent->ProcessInfectionOutcomes(timestep, {});
    agent->UpdateContactReports(contact_reports, contact_report_broker.get());
    EXPECT_THAT(agent->GetContactSummary(),
                Eq(ContactSummary{
                    .retention_horizon = absl::FromUnixSeconds(172800LL),
                    .latest_contact_time = absl::FromUnixSeconds(171000LL)}));
  }
  {
    contacts = {{
        .other_uuid = 314LL,
        .exposure{.start_time = absl::FromUnixSeconds(178200LL),
                  .duration = absl::Hours(1LL)},
    }};
    const Timestep timestep(absl::FromUnixSeconds(180000LL), absl::Hours(22));
    agent->ProcessInfectionOutcomes(
        timestep, {InfectionOutcomeFromContact(kUuid, *contacts.begin())});
    agent->UpdateContactReports(contact_reports, contact_report_broker.get());
    EXPECT_THAT(agent->GetContactSummary(),
                Eq(ContactSummary{
                    .retention_horizon = absl::FromUnixSeconds(176400LL),
                    .latest_contact_time = absl::FromUnixSeconds(181800LL)}));
  }
}

TEST(SEIRAgentTest, UpdateContactReportsRejectsWrongUuid) {
  auto transition_model = absl::make_unique<MockTransitionModel>();
  auto visit_generator = absl::make_unique<MockVisitGenerator>();
  MockTransmissionModel transmission_model;
  auto public_policy = NewNoOpPolicy();
  const int64 kUuid = 42LL;
  const std::vector<ContactReport> contact_reports{
      {.from_agent_uuid = kUuid, .to_agent_uuid = kUuid + 1}};
  auto agent = SEIRAgent::CreateSusceptible(
      kUuid, &transmission_model, std::move(transition_model),
      std::move(visit_generator), public_policy.get());
  auto broker = absl::make_unique<MockBroker<ContactReport>>();
  ASSERT_DEBUG_DEATH(agent->UpdateContactReports(contact_reports, broker.get()),
                     "");
}

}  // namespace
}  // namespace pandemic
