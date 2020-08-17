#ifndef AGENT_BASED_EPIDEMIC_SIM_APPLICATIONS_HOME_WORK_LEARNING_HISTORY_AND_TESTING_OBSERVER_H_
#define AGENT_BASED_EPIDEMIC_SIM_APPLICATIONS_HOME_WORK_LEARNING_HISTORY_AND_TESTING_OBSERVER_H_

#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "absl/status/status.h"
#include "absl/types/span.h"
#include "agent_based_epidemic_sim/core/agent.h"
#include "agent_based_epidemic_sim/core/event.h"
#include "agent_based_epidemic_sim/core/observer.h"

namespace abesim {

struct HealthTransitionsAndTestResults {
  int64 agent_uuid;
  std::vector<HealthTransition> health_transitions;
  std::vector<TestResult> test_results;
};

// This is an observer for writing out contacts between agents in a format
// compliant with the documentation in (broken link).
class LearningHistoryAndTestingObserver : public AgentInfectionObserver {
 public:
  explicit LearningHistoryAndTestingObserver(const Timestep& timestep);

  void Observe(const Agent& agent,
               absl::Span<const InfectionOutcome> outcomes) override;

 private:
  friend class LearningHistoryAndTestingObserverFactory;

  const Timestep timestep_;
  std::vector<HealthTransitionsAndTestResults> history_and_tests_;
};

class LearningHistoryAndTestingObserverFactory
    : public ObserverFactory<LearningHistoryAndTestingObserver> {
 public:
  explicit LearningHistoryAndTestingObserverFactory(
      absl::string_view output_pattern);

  void Aggregate(
      const Timestep& timestep,
      absl::Span<std::unique_ptr<LearningHistoryAndTestingObserver> const>
          observers) override;
  std::unique_ptr<LearningHistoryAndTestingObserver> MakeObserver(
      const Timestep& timestep) const override;

  absl::Status status() const { return status_; }

 private:
  absl::Status status_;
  std::string output_pattern_;
};

}  // namespace abesim

#endif  // AGENT_BASED_EPIDEMIC_SIM_APPLICATIONS_HOME_WORK_LEARNING_HISTORY_AND_TESTING_OBSERVER_H_
