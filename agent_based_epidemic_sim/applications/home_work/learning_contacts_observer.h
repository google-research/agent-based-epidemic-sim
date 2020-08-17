#ifndef AGENT_BASED_EPIDEMIC_SIM_APPLICATIONS_HOME_WORK_LEARNING_CONTACTS_OBSERVER_H_
#define AGENT_BASED_EPIDEMIC_SIM_APPLICATIONS_HOME_WORK_LEARNING_CONTACTS_OBSERVER_H_

#include <memory>
#include <string>

#include "absl/status/status.h"
#include "absl/types/span.h"
#include "agent_based_epidemic_sim/core/agent.h"
#include "agent_based_epidemic_sim/core/event.h"
#include "agent_based_epidemic_sim/core/observer.h"

namespace abesim {

// This is an observer for writing out contacts between agents in a format
// compliant with the documentation in (broken link).
class LearningContactsObserver : public AgentInfectionObserver {
 public:
  explicit LearningContactsObserver();

  void Observe(const Agent& agent,
               absl::Span<const InfectionOutcome> outcomes) override;

 private:
  friend class LearningContactsObserverFactory;

  std::list<InfectionOutcome> outcomes_;
};

class LearningContactsObserverFactory
    : public ObserverFactory<LearningContactsObserver> {
 public:
  explicit LearningContactsObserverFactory(absl::string_view output_pattern);

  void Aggregate(const Timestep& timestep,
                 absl::Span<std::unique_ptr<LearningContactsObserver> const>
                     observers) override;
  std::unique_ptr<LearningContactsObserver> MakeObserver(
      const Timestep& timestep) const override;

  absl::Status status() const { return status_; }

 private:
  absl::Status status_;
  std::string output_pattern_;
};

}  // namespace abesim

#endif  // AGENT_BASED_EPIDEMIC_SIM_APPLICATIONS_HOME_WORK_LEARNING_CONTACTS_OBSERVER_H_
