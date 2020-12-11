#ifndef AGENT_BASED_EPIDEMIC_SIM_APPLICATIONS_RISK_LEARNING_OBSERVERS_H_
#define AGENT_BASED_EPIDEMIC_SIM_APPLICATIONS_RISK_LEARNING_OBSERVERS_H_

#include <memory>

#include "absl/strings/string_view.h"
#include "absl/types/span.h"
#include "agent_based_epidemic_sim/applications/risk_learning/exposures_per_test_result.pb.h"
#include "agent_based_epidemic_sim/core/agent.h"
#include "agent_based_epidemic_sim/core/enum_indexed_array.h"
#include "agent_based_epidemic_sim/core/observer.h"
#include "agent_based_epidemic_sim/core/timestep.h"
#include "agent_based_epidemic_sim/port/file_utils.h"
#include "agent_based_epidemic_sim/util/records.h"

namespace abesim {

using HealthStateCounts =
    EnumIndexedArray<int64, HealthState::State, HealthState::State_ARRAYSIZE>;

class SummaryObserver : public AgentInfectionObserver {
 public:
  explicit SummaryObserver(Timestep timestep);
  void Observe(const Agent& agent,
               absl::Span<const InfectionOutcome> outcomes) override;

 private:
  friend class SummaryObserverFactory;
  const Timestep timestep_;
  HealthStateCounts counts_;
};

// SummaryObserverFactory writes summary statistics to the given file for
// every simulated timestep.
class SummaryObserverFactory : public ObserverFactory<SummaryObserver> {
 public:
  SummaryObserverFactory(absl::string_view summary_filename);
  ~SummaryObserverFactory();

  std::unique_ptr<SummaryObserver> MakeObserver(
      const Timestep& timestep) const override;

  void Aggregate(
      const Timestep& timestep,
      absl::Span<std::unique_ptr<SummaryObserver> const> observers) override;

  static constexpr std::array kOutputStates = {
      HealthState::SUSCEPTIBLE,
      HealthState::ASYMPTOMATIC,
      HealthState::PRE_SYMPTOMATIC_MILD,
      HealthState::PRE_SYMPTOMATIC_SEVERE,
      HealthState::SYMPTOMATIC_MILD,
      HealthState::SYMPTOMATIC_SEVERE,
      HealthState::SYMPTOMATIC_HOSPITALIZED,
      HealthState::SYMPTOMATIC_CRITICAL,
      HealthState::SYMPTOMATIC_HOSPITALIZED_RECOVERING,
      HealthState::RECOVERED,
      HealthState::REMOVED,
  };

 private:
  std::unique_ptr<file::FileWriter> writer_;
};

class LearningObserver : public AgentInfectionObserver {
 public:
  explicit LearningObserver(Timestep timestep);
  void Observe(const Agent& agent,
               absl::Span<const InfectionOutcome> outcomes) override;

 private:
  friend class LearningObserverFactory;
  const Timestep timestep_;
  std::vector<ExposuresPerTestResult::ExposureResult> results_;
};

// LearningObserverFactory writes data for training risk score models.  It
// writes one new file for every timestep, the names of the files are generated
// by appending timestep information to the given base filename.
class LearningObserverFactory : public ObserverFactory<LearningObserver> {
 public:
  LearningObserverFactory(absl::string_view learning_filename, int num_workers);
  ~LearningObserverFactory();

  std::unique_ptr<LearningObserver> MakeObserver(
      const Timestep& timestep) const override;

  void Aggregate(
      const Timestep& timestep,
      absl::Span<std::unique_ptr<LearningObserver> const> observers) override;

 private:
  riegeli::RecordWriter<RiegeliBytesSink> writer_;
};

}  // namespace abesim

#endif  // AGENT_BASED_EPIDEMIC_SIM_APPLICATIONS_RISK_LEARNING_OBSERVERS_H_
