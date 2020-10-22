#include "agent_based_epidemic_sim/applications/risk_learning/observers.h"

#include <initializer_list>
#include <memory>

#include "absl/time/time.h"
#include "agent_based_epidemic_sim/applications/risk_learning/exposures_per_test_result.pb.h"
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

std::unique_ptr<Agent> MakeAgentInState(HealthState::State state) {
  auto agent = absl::make_unique<testing::NiceMock<MockAgent>>();
  ON_CALL(*agent, CurrentHealthState()).WillByDefault(testing::Return(state));
  return agent;
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
    auto add_agents = [&observers](HealthState::State state, int n) {
      auto agent = MakeAgentInState(state);
      for (int i = 0; i < n; ++i) {
        observers[i % 2]->Observe(*agent, {});
      }
    };
    for (int i = 0; i < SummaryObserverFactory::kOutputStates.size(); ++i) {
      add_agents(SummaryObserverFactory::kOutputStates[i],
                 SummaryObserverFactory::kOutputStates.size() - i);
    }
    factory.Aggregate(timestep, observers);

    timestep.Advance();
    observers.clear();
    observers.push_back(factory.MakeObserver(timestep));
    observers.push_back(factory.MakeObserver(timestep));
    for (int i = 0; i < SummaryObserverFactory::kOutputStates.size(); ++i) {
      add_agents(SummaryObserverFactory::kOutputStates[i], i + 1);
    }
    factory.Aggregate(timestep, observers);
  }
  std::string output;
  PANDEMIC_ASSERT_OK(file::GetContents(summary_filename, &output));
  EXPECT_EQ(output,
            "DATE, SUSCEPTIBLE, ASYMPTOMATIC, PRE_SYMPTOMATIC_MILD, "
            "PRE_SYMPTOMATIC_SEVERE, SYMPTOMATIC_MILD, SYMPTOMATIC_SEVERE, "
            "SYMPTOMATIC_HOSPITALIZED, SYMPTOMATIC_CRITICAL, "
            "SYMPTOMATIC_HOSPITALIZED_RECOVERING, RECOVERED, REMOVED\n"
            "1970-01-01, 11, 10, 9, 8, 7, 6, 5, 4, 3, 2, 1\n"
            "1970-01-02, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11\n");
}

absl::Time TestTime(int day, int hour) {
  return absl::UnixEpoch() + absl::Hours(24 * day + hour);
}

class LearningObserverTest : public testing::Test {
 protected:
  LearningObserverTest()
      : filename_(absl::StrCat(getenv("TEST_TMPDIR"), "/", "learning")),
        factory_(absl::make_unique<LearningObserverFactory>(filename_)),
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
      .WillByDefault(Return(TestResult{
          .time_requested = timestep_.start_time() - absl::Hours(16),
          .time_received = timestep_.start_time() + absl::Hours(12),
          .outcome = TestOutcome::POSITIVE,
      }));
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
      ParseTextProtoOrDie<ExposuresPerTestResult::ExposureResult>(R"(
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
        }
        exposures {
          exposure_time { seconds: 57600 }
          duration { seconds: 900 }
          distance: 2
          exposure_type: UNCONFIRMED
          source_uuid: 789
        }
        infection_onset_time { seconds: 86400 }
      )");
  EXPECT_EQ(results.value()[0].DebugString(), expected.DebugString());
}  // namespace

}  // namespace
}  // namespace abesim
