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

#include "agent_based_epidemic_sim/applications/home_work/observer.h"

#include "absl/container/flat_hash_map.h"
#include "absl/time/time.h"
#include "agent_based_epidemic_sim/core/agent.h"
#include "agent_based_epidemic_sim/core/broker.h"
#include "agent_based_epidemic_sim/core/event.h"
#include "agent_based_epidemic_sim/core/location.h"
#include "agent_based_epidemic_sim/core/pandemic.pb.h"
#include "agent_based_epidemic_sim/port/status_matchers.h"
#include "gmock/gmock.h"
#include "gtest/gtest.h"

namespace abesim {
namespace {

class MemFileWriterImpl : public file::FileWriter {
 public:
  explicit MemFileWriterImpl(std::string* output) : output_(output) {}

  absl::Status WriteString(absl::string_view content) override {
    absl::StrAppend(output_, content);
    return absl::OkStatus();
  }

  absl::Status Close() override { return absl::OkStatus(); }

 private:
  std::string* output_;
};

const char* kExpectedHeaders =
    "timestep_end,agents,"
    "SUSCEPTIBLE,EXPOSED,INFECTIOUS,RECOVERED,ASYMPTOMATIC,"
    "PRE_SYMPTOMATIC_MILD,PRE_SYMPTOMATIC_SEVERE,SYMPTOMATIC_MILD,"
    "SYMPTOMATIC_SEVERE,SYMPTOMATIC_HOSPITALIZED,SYMPTOMATIC_CRITICAL,"
    "SYMPTOMATIC_HOSPITALIZED_RECOVERING,REMOVED,home_0,home_1h,"
    "home_2h,home_4h,home_8h,home_16h,work_0,work_1h,work_2h,work_4h,work_8h,"
    "work_16h,contact_1,contact_2,contact_4,contact_8,contact_16,"
    "contact_32,contact_64,contact_128,contact_256,contact_512\n";

class MockAgent : public Agent {
 public:
  MOCK_METHOD(int64, uuid, (), (const, override));
  MOCK_METHOD(void, ComputeVisits,
              (const Timestep& timestep, Broker<Visit>* visit_broker),
              (const, override));
  MOCK_METHOD(void, ProcessInfectionOutcomes,
              (const Timestep& timestep,
               absl::Span<const InfectionOutcome> infection_outcomes),
              (override));
  MOCK_METHOD(void, UpdateContactReports,
              (const Timestep& timestep,
               absl::Span<const ContactReport> symptom_reports,
               Broker<ContactReport>* symptom_broker),
              (override));
  MOCK_METHOD(HealthState::State, CurrentHealthState, (), (const, override));
  MOCK_METHOD(TestResult, CurrentTestResult, (const Timestep&),
              (const, override));
  MOCK_METHOD(absl::Span<const HealthTransition>, HealthTransitions, (),
              (const, override));
};

class MockLocation : public Location {
 public:
  MOCK_METHOD(int64, uuid, (), (const, override));

  // Process a set of visits and write InfectionOutcomes to the given
  // infection_broker.  If observer != nullptr, then observer should be called
  // for each visit with a list of corresponding contacts.
  MOCK_METHOD(void, ProcessVisits,
              (absl::Span<const Visit> visits,
               Broker<InfectionOutcome>* infection_broker),
              (override));
};

std::unique_ptr<MockAgent> MakeAgent(int64 uuid, HealthState::State state) {
  auto agent = absl::make_unique<MockAgent>();
  EXPECT_CALL(*agent, uuid())
      .Times(testing::AnyNumber())
      .WillRepeatedly(testing::Return(uuid));
  EXPECT_CALL(*agent, CurrentHealthState())
      .Times(testing::AnyNumber())
      .WillRepeatedly(testing::Return(state));
  return agent;
}

std::unique_ptr<MockLocation> MakeLocation(int64 uuid) {
  auto location = absl::make_unique<MockLocation>();
  EXPECT_CALL(*location, uuid())
      .Times(testing::AnyNumber())
      .WillRepeatedly(testing::Return(uuid));
  return location;
}

absl::Time TestHour(int hours) {
  return absl::UnixEpoch() + absl::Hours(hours);
}

TEST(HomeWorkSimulationObserverTest, ZerosReturnedForNoObservations) {
  Timestep t(absl::UnixEpoch(), absl::Hours(24));

  std::string output;
  auto file = absl::make_unique<MemFileWriterImpl>(&output);

  {
    HomeWorkSimulationObserverFactory observer_factory(
        file.get(),
        [](int64 uuid) {
          return uuid == 0 ? LocationType::kHome : LocationType::kWork;
        },
        {});
    std::vector<std::unique_ptr<HomeWorkSimulationObserver>> observers;
    Timestep timestep(absl::UnixEpoch(), absl::Hours(24));
    observers.push_back(observer_factory.MakeObserver(timestep));
    observers.push_back(observer_factory.MakeObserver(timestep));
    observer_factory.Aggregate(t, observers);

    std::string expected = kExpectedHeaders;
    expected +=
        "86400,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,"
        "0,0,0,0,0,0,0,0,0\n";
    EXPECT_EQ(output, expected);
  }

  PANDEMIC_ASSERT_OK(file->Close());
}

TEST(HomeWorkSimulationObserverTest, PassthroughFields) {
  Timestep t(absl::UnixEpoch(), absl::Hours(24));

  std::string output;
  auto file = absl::make_unique<MemFileWriterImpl>(&output);

  std::vector<std::pair<std::string, std::string>> passthrough = {{"k1", "v1"},
                                                                  {"k2", "v2"}};
  {
    HomeWorkSimulationObserverFactory observer_factory(
        file.get(),
        [](int64 uuid) {
          return uuid == 0 ? LocationType::kHome : LocationType::kWork;
        },
        passthrough);
    std::vector<std::unique_ptr<HomeWorkSimulationObserver>> observers;
    Timestep timestep(absl::UnixEpoch(), absl::Hours(24));
    observers.push_back(observer_factory.MakeObserver(timestep));
    observers.push_back(observer_factory.MakeObserver(timestep));
    observer_factory.Aggregate(t, observers);

    std::string expected = std::string("k1,k2,") + kExpectedHeaders;
    expected +=
        "v1,v2,86400,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,"
        "0,0,0,0,0,0,0,0,0\n";
    EXPECT_EQ(output, expected);
  }

  PANDEMIC_ASSERT_OK(file->Close());
}

TEST(HomeWorkSimulationObserverTest, CorrectValuesForObservations) {
  Timestep t(absl::UnixEpoch(), absl::Hours(24));

  std::string output;
  auto file = absl::make_unique<MemFileWriterImpl>(&output);

  {
    HomeWorkSimulationObserverFactory observer_factory(
        file.get(),
        [](int64 uuid) {
          return uuid == 0 ? LocationType::kHome : LocationType::kWork;
        },
        {});
    std::vector<std::unique_ptr<HomeWorkSimulationObserver>> observers;
    Timestep timestep(absl::UnixEpoch(), absl::Hours(24));
    observers.push_back(observer_factory.MakeObserver(timestep));
    observers.push_back(observer_factory.MakeObserver(timestep));

    auto home = MakeLocation(0);
    auto work = MakeLocation(1);

    std::vector<Visit> home_visits, work_visits;
    for (int64 i = 0; i < 4; ++i) {
      int64 uuid = i;
      home_visits.push_back({
          .location_uuid = 0,
          .agent_uuid = uuid,
          .start_time = TestHour(0),
          .end_time = TestHour(12),
      });
      work_visits.push_back({
          .location_uuid = 1,
          .agent_uuid = uuid,
          .start_time = TestHour(12),
          .end_time = TestHour(16),
      });
      std::vector<InfectionOutcome> outcomes;
      for (int j = 0; j < 4; ++j) {
        if (j == i) continue;
        outcomes.push_back({
            .agent_uuid = uuid,
            .exposure_type = InfectionOutcomeProto::CONTACT,
            .source_uuid = j,
        });
      }
      observers[0]->Observe(*MakeAgent(uuid, HealthState::SUSCEPTIBLE),
                            outcomes);
    }
    for (int64 i = 0; i < 3; ++i) {
      int64 uuid = 4 + i;
      home_visits.push_back({
          .location_uuid = 0,
          .agent_uuid = uuid,
          .start_time = TestHour(0),
          .end_time = TestHour(3),
      });
      work_visits.push_back({
          .location_uuid = 1,
          .agent_uuid = uuid,
          .start_time = TestHour(3),
          .end_time = TestHour(11),
      });
      std::vector<InfectionOutcome> outcomes;
      for (int j = 0; j < 3; ++j) {
        if (j == i) continue;
        outcomes.push_back({
            .agent_uuid = uuid,
            .exposure_type = InfectionOutcomeProto::CONTACT,
            .source_uuid = 4 + j,
        });
      }
      observers[0]->Observe(*MakeAgent(uuid, HealthState::EXPOSED), outcomes);
    }
    for (int64 i = 0; i < 2; ++i) {
      int64 uuid = 7 + i;
      home_visits.push_back({
          .location_uuid = 0,
          .agent_uuid = uuid,
          .start_time = TestHour(0),
          .end_time = TestHour(0),
      });
      work_visits.push_back({
          .location_uuid = 1,
          .agent_uuid = uuid,
          .start_time = TestHour(0),
          .end_time = TestHour(24),
      });
      std::vector<InfectionOutcome> outcomes;
      for (int j = 0; j < 2; ++j) {
        if (j == i) continue;
        outcomes.push_back({
            .agent_uuid = uuid,
            .exposure_type = InfectionOutcomeProto::CONTACT,
            .source_uuid = 7 + j,
        });
      }
      observers[1]->Observe(*MakeAgent(uuid, HealthState::INFECTIOUS),
                            outcomes);
    }
    observers[1]->Observe(*MakeAgent(9, HealthState::RECOVERED), {});
    // Doesn't visit anywhere.

    observers[0]->Observe(*home, home_visits);
    observers[1]->Observe(*work, work_visits);

    observer_factory.Aggregate(t, observers);
    std::string expected = kExpectedHeaders;
    expected +=
        "86400,10,4,3,2,1,0,0,0,0,0,0,0,0,0,0,0,"
        "3,0,4,0,0,0,0,4,3,2,2,3,4,0,0,0,0,0,0,0\n";
    EXPECT_EQ(output, expected);

    PANDEMIC_ASSERT_OK(observer_factory.status());
  }

  PANDEMIC_ASSERT_OK(file->Close());
}

}  // namespace
}  // namespace abesim
