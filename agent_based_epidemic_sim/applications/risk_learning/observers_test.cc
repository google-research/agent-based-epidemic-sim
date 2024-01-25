#include "agent_based_epidemic_sim/applications/risk_learning/observers.h"

#include <initializer_list>
#include <memory>

#include "absl/strings/str_cat.h"
#include "absl/time/time.h"
#include "agent_based_epidemic_sim/applications/risk_learning/exposures_per_test_result.pb.h"
#include "agent_based_epidemic_sim/applications/risk_learning/hazard_transmission_model.h"
#include "agent_based_epidemic_sim/core/exposure_store.h"
#include "agent_based_epidemic_sim/core/pandemic.pb.h"
#include "agent_based_epidemic_sim/core/parse_text_proto.h"
#include "agent_based_epidemic_sim/port/file_utils.h"
#include "agent_based_epidemic_sim/port/status_matchers.h"
#include "agent_based_epidemic_sim/port/time_proto_util.h"
#include "agent_based_epidemic_sim/util/records.h"
#include "agent_based_epidemic_sim/util/test_util.h"
#include "gmock/gmock.h"
#include "gtest/gtest.h"

namespace abesim {
namespace {

using ::testing::Return;

std::unique_ptr<Agent> MakeAgentInState(HealthState::State state,
                                        const Timestep& timestep) {
  auto agent = absl::make_unique<testing::NiceMock<MockAgent>>();
  ON_CALL(*agent, CurrentHealthState()).WillByDefault(testing::Return(state));
  ON_CALL(*agent, CurrentTestResult(testing::_))
      .WillByDefault(testing::Return(TestResult{
          .time_received = timestep.start_time(),
          .outcome = IsSymptomaticState(state) ? TestOutcome::POSITIVE
                                               : TestOutcome::NEGATIVE}));
  ON_CALL(*agent, HealthTransitions())
      .WillByDefault(testing::Return(std::vector<HealthTransition>{
          HealthTransition{.time = timestep.start_time()}}));
  return agent;
}

TEST(SummaryObserverTest, CreationOfSummaryObserverFactoryCanOverwriteFiles) {
  std::string summary_filename =
      absl::StrCat(getenv("TEST_TMPDIR"), "/", "summary");
  { SummaryObserverFactory old_factory(summary_filename); }
  {
    SummaryObserverFactory factory(summary_filename);
    Timestep timestep(absl::UnixEpoch(), absl::Hours(24));
    std::vector<std::unique_ptr<SummaryObserver>> observers;
    observers.push_back(factory.MakeObserver(timestep));
    observers.push_back(factory.MakeObserver(timestep));
    auto add_agents = [&observers](HealthState::State state, int n,
                                   const Timestep& timestep) {
      auto agent = MakeAgentInState(state, timestep);
      for (int i = 0; i < n; ++i) {
        observers[i % 2]->Observe(*agent, {});
      }
    };
    for (int i = 0; i < SummaryObserverFactory::kOutputStates.size(); ++i) {
      add_agents(SummaryObserverFactory::kOutputStates[i],
                 SummaryObserverFactory::kOutputStates.size() - i, timestep);
    }
    factory.Aggregate(timestep, observers);

    auto first_timestep = timestep;
    timestep.Advance();
    observers.clear();
    observers.push_back(factory.MakeObserver(timestep));
    observers.push_back(factory.MakeObserver(timestep));
    for (int i = 0; i < SummaryObserverFactory::kOutputStates.size(); ++i) {
      add_agents(SummaryObserverFactory::kOutputStates[i], i + 1,
                 i % 2 ? first_timestep : timestep);
    }
    factory.Aggregate(timestep, observers);
  }
  std::string output;
  PANDEMIC_ASSERT_OK(file::GetContents(summary_filename, &output));
  EXPECT_EQ(output,
            "DATE, SUSCEPTIBLE, ASYMPTOMATIC, PRE_SYMPTOMATIC_MILD, "
            "PRE_SYMPTOMATIC_SEVERE, SYMPTOMATIC_MILD, SYMPTOMATIC_SEVERE, "
            "SYMPTOMATIC_HOSPITALIZED, SYMPTOMATIC_CRITICAL, "
            "SYMPTOMATIC_HOSPITALIZED_RECOVERING, RECOVERED, REMOVED, "
            "NEWLY_SYMPTOMATIC_MILD, NEWLY_SYMPTOMATIC_SEVERE, "
            "NEWLY_TEST_POSITIVE\n"
            "1970-01-01, 11, 10, 9, 8, 7, 6, 5, 4, 3, 2, 1, 7, 6, 25\n"
            "1970-01-02, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 5, 0, 21\n");
}

TEST(SummaryObserverTest, RecordsOutputSuccessfully) {
  std::string summary_filename =
      absl::StrCat(getenv("TEST_TMPDIR"), "/", "summary");
  {
    SummaryObserverFactory factory(summary_filename);
    Timestep timestep(absl::UnixEpoch(), absl::Hours(24));
    std::vector<std::unique_ptr<SummaryObserver>> observers;
    observers.push_back(factory.MakeObserver(timestep));
    observers.push_back(factory.MakeObserver(timestep));
    auto add_agents = [&observers](HealthState::State state, int n,
                                   const Timestep& timestep) {
      auto agent = MakeAgentInState(state, timestep);
      for (int i = 0; i < n; ++i) {
        observers[i % 2]->Observe(*agent, {});
      }
    };
    for (int i = 0; i < SummaryObserverFactory::kOutputStates.size(); ++i) {
      add_agents(SummaryObserverFactory::kOutputStates[i],
                 SummaryObserverFactory::kOutputStates.size() - i, timestep);
    }
    factory.Aggregate(timestep, observers);

    auto first_timestep = timestep;
    timestep.Advance();
    observers.clear();
    observers.push_back(factory.MakeObserver(timestep));
    observers.push_back(factory.MakeObserver(timestep));
    for (int i = 0; i < SummaryObserverFactory::kOutputStates.size(); ++i) {
      add_agents(SummaryObserverFactory::kOutputStates[i], i + 1,
                 i % 2 ? first_timestep : timestep);
    }
    factory.Aggregate(timestep, observers);
  }
  std::string output;
  PANDEMIC_ASSERT_OK(file::GetContents(summary_filename, &output));
  EXPECT_EQ(output,
            "DATE, SUSCEPTIBLE, ASYMPTOMATIC, PRE_SYMPTOMATIC_MILD, "
            "PRE_SYMPTOMATIC_SEVERE, SYMPTOMATIC_MILD, SYMPTOMATIC_SEVERE, "
            "SYMPTOMATIC_HOSPITALIZED, SYMPTOMATIC_CRITICAL, "
            "SYMPTOMATIC_HOSPITALIZED_RECOVERING, RECOVERED, REMOVED, "
            "NEWLY_SYMPTOMATIC_MILD, NEWLY_SYMPTOMATIC_SEVERE, "
            "NEWLY_TEST_POSITIVE\n"
            "1970-01-01, 11, 10, 9, 8, 7, 6, 5, 4, 3, 2, 1, 7, 6, 25\n"
            "1970-01-02, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 5, 0, 21\n");
}

absl::Time TestTime(int day, int hour) {
  return absl::UnixEpoch() + absl::Hours(24 * day + hour);
}

class LearningObserverTest : public testing::Test {
 protected:
  LearningObserverTest()
      : filename_(absl::StrCat(getenv("TEST_TMPDIR"), "/", "learning")),
        hazard_transmission_model_(
            absl::make_unique<HazardTransmissionModel>()),
        factory_(absl::make_unique<LearningObserverFactory>(
            filename_,
            /*parallelism=*/1,
            /*reporting_delay=*/absl::ZeroDuration(),
            hazard_transmission_model_.get())),
        timestep_(TestTime(3, 0), absl::Hours(24)) {}

  LearningObserver& Observer() {
    observers_.push_back(factory_->MakeObserver(timestep_));
    return *observers_.back();
  }

  absl::StatusOr<std::vector<ExposuresPerTestResult::ExposureResult>>
  GetResults() {
    factory_->Aggregate(timestep_, observers_);
    factory_.reset();
    std::vector<ExposuresPerTestResult::ExposureResult> results;
    auto reader = MakeRecordReader(filename_);
    ExposuresPerTestResult::ExposureResult result;
    while (reader.ReadRecord(result)) {
      results.push_back(result);
    }
    if (!reader.status().ok()) return reader.status();
    return results;
  }

  const std::string filename_;
  std::unique_ptr<HazardTransmissionModel> hazard_transmission_model_;
  std::unique_ptr<LearningObserverFactory> factory_;
  Timestep timestep_;
  std::vector<std::unique_ptr<LearningObserver>> observers_;
};

TEST_F(LearningObserverTest, DoesNotRecordTestsForOtherDays) {
  auto make_agent = [this](absl::Time received_time) {
    auto agent = absl::make_unique<testing::NiceMock<MockAgent>>();
    ON_CALL(*agent, CurrentTestResult(timestep_))
        .WillByDefault(
            testing::Return(TestResult{.time_received = received_time}));
    return agent;
  };
  auto& observer = Observer();
  std::vector test_times = {
      timestep_.start_time() - absl::Hours(1),
      timestep_.start_time() + absl::Hours(1),
      timestep_.end_time() - absl::Hours(1),
      timestep_.end_time() + absl::Hours(1),
  };
  for (const auto& time : test_times) {
    observer.Observe(*make_agent(time), {});
  }
  auto results = GetResults();
  PANDEMIC_ASSERT_OK(results);
  EXPECT_EQ(results.value().size(), 2);
  EXPECT_EQ(*DecodeGoogleApiProto(results.value()[0].test_received_time()),
            test_times[1]);
  EXPECT_EQ(*DecodeGoogleApiProto(results.value()[1].test_received_time()),
            test_times[2]);
}

TEST_F(LearningObserverTest, RecordsAllFields) {
  testing::NiceMock<MockAgent> agent;
  ON_CALL(agent, uuid()).WillByDefault(Return(12345));
  ON_CALL(agent, CurrentTestResult(timestep_))
      .WillByDefault(Return(
          TestResult{.time_requested = timestep_.start_time() - absl::Hours(16),
                     .time_received = timestep_.start_time() + absl::Hours(12),
                     .outcome = TestOutcome::POSITIVE,
                     .hazard = 1}));
  ON_CALL(agent, infection_onset())
      .WillByDefault(Return(timestep_.start_time() - absl::Hours(24 * 2)));

  ExposureStore exposures;
  ON_CALL(agent, exposure_store()).WillByDefault(Return(&exposures));
  exposures.AddExposures({
      {
          .exposure =
              {
                  .start_time =
                      timestep_.start_time() - absl::Hours(24 * 2 + 8),
                  .duration = absl::Seconds(900),
                  .distance = 2,
                  .infectivity = 0,
              },
          .source_uuid = 789,
      },
      {
          .exposure =
              {
                  .start_time =
                      timestep_.start_time() - absl::Hours(24 * 2 - 8),
                  .duration = absl::Seconds(900),
                  .distance = 6,
                  .infectivity = 1,
                  .symptom_factor = 1,
                  .susceptibility = 1,
                  .location_transmissibility = 1,
              },
          .source_uuid = 654,
      },
  });
  exposures.ProcessNotification(
      {
          .from_agent_uuid = 654,
          .initial_symptom_onset_time =
              timestep_.start_time() - absl::Hours(24 * 2 - 12),
      },
      [](const Exposure&) {});

  Observer().Observe(agent, {});

  auto results = GetResults();
  PANDEMIC_ASSERT_OK(results);
  EXPECT_EQ(results.value().size(), 1);
  auto expected =
      ParseTextProtoOrDie<ExposuresPerTestResult::ExposureResult>(R"pb(
        agent_uuid: 12345
        outcome: POSITIVE
        test_administered_time { seconds: 201600 }
        test_received_time { seconds: 302400 }
        exposures {
          exposure_time { seconds: 115200 }
          duration_since_symptom_onset { seconds: -14400 }
          duration { seconds: 900 }
          distance: 6
          exposure_type: CONFIRMED
          source_uuid: 654
          infectivity: 1
          dose: 1.2475902
        }
        exposures {
          exposure_time { seconds: 57600 }
          duration { seconds: 900 }
          distance: 2
          exposure_type: UNCONFIRMED
          source_uuid: 789
        }
        infection_onset_time { seconds: 86400 }
        hazard: 1.0
      )pb");
  EXPECT_EQ(absl::StrCat(results.value()[0]), absl::StrCat(expected));
}  // namespace

}  // namespace
}  // namespace abesim
