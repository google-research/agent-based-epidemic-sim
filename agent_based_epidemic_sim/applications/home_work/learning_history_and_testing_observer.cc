#include "agent_based_epidemic_sim/applications/home_work/learning_history_and_testing_observer.h"

#include "absl/strings/str_cat.h"
#include "absl/strings/str_join.h"
#include "absl/strings/substitute.h"
#include "absl/time/time.h"
#include "absl/types/span.h"
#include "agent_based_epidemic_sim/core/agent.h"
#include "agent_based_epidemic_sim/core/event.h"
#include "agent_based_epidemic_sim/core/pandemic.pb.h"
#include "agent_based_epidemic_sim/core/timestep.h"
#include "agent_based_epidemic_sim/port/file_utils.h"

namespace abesim {

LearningHistoryAndTestingObserver::LearningHistoryAndTestingObserver(
    const Timestep& timestep)
    : timestep_(timestep) {}

void LearningHistoryAndTestingObserver::Observe(
    const Agent& agent, absl::Span<const InfectionOutcome> outcomes) {
  HealthTransitionsAndTestResults output;
  output.agent_uuid = agent.uuid();
  for (const auto& health_transition : agent.HealthTransitions()) {
    output.health_transitions.push_back(health_transition);
  }
  output.test_results.push_back(agent.CurrentTestResult(timestep_));
  history_and_tests_.push_back(output);
}

LearningHistoryAndTestingObserverFactory::
    LearningHistoryAndTestingObserverFactory(absl::string_view output_pattern)
    : output_pattern_(output_pattern) {}

void LearningHistoryAndTestingObserverFactory::Aggregate(
    const Timestep& timestep,
    absl::Span<std::unique_ptr<LearningHistoryAndTestingObserver> const>
        observers) {
  auto history_writer =
      file::OpenOrDie(absl::StrCat(output_pattern_, "_history.csv"));
  auto tests_writer =
      file::OpenOrDie(absl::StrCat(output_pattern_, "_tests.csv"));
  for (const auto& observer : observers) {
    for (const auto& history_and_tests : observer->history_and_tests_) {
      const auto agent_uuid = history_and_tests.agent_uuid;
      std::string history_line = absl::StrCat(agent_uuid);
      std::string test_line = absl::StrCat(agent_uuid);
      HealthState::State last_state = HealthState::SUSCEPTIBLE;
      for (const auto& health_transition :
           history_and_tests.health_transitions) {
        if (health_transition.health_state == last_state) {
          continue;
        }
        absl::StrAppend(&history_line, ",", health_transition.health_state, ",",
                        absl::FormatTime(health_transition.time));
        last_state = health_transition.health_state;
      }
      for (const auto& test_result : history_and_tests.test_results) {
        absl::StrAppend(&test_line, ",", test_result.probability, ",",
                        absl::FormatTime(test_result.time_received));
      }
      absl::StrAppend(&history_line, "\n");
      absl::StrAppend(&test_line, "\n");
      status_.Update(history_writer->WriteString(history_line));
      status_.Update(tests_writer->WriteString(test_line));
    }
  }
}

std::unique_ptr<LearningHistoryAndTestingObserver>
LearningHistoryAndTestingObserverFactory::MakeObserver(
    const Timestep& timestep) const {
  return absl::make_unique<LearningHistoryAndTestingObserver>(timestep);
}

}  // namespace abesim
