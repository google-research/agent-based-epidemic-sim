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
#include "agent_based_epidemic_sim/util/histogram.h"
#include "agent_based_epidemic_sim/util/records.h"

namespace abesim {
namespace internal {
constexpr int kHazardHistogramBuckets = 100;
}  // namespace internal

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
  int newly_symptomatic_mild_ = 0;
  int newly_symptomatic_severe_ = 0;
  int newly_test_positive_ = 0;
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
  std::string header_;
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

class HazardHistogramObserver : public AgentInfectionObserver {
 public:
  explicit HazardHistogramObserver(Timestep timestep);
  void Observe(const Agent& agent,
               absl::Span<const InfectionOutcome> outcomes) override;

 private:
  friend class HazardHistogramObserverFactory;
  Timestep timestep_;
  std::vector<float> hazards_;
};

class HazardHistogramObserverFactory
    : public ObserverFactory<HazardHistogramObserver> {
 public:
  ~HazardHistogramObserverFactory();
  HazardHistogramObserverFactory(absl::string_view hazard_histogram_filename);
  std::unique_ptr<HazardHistogramObserver> MakeObserver(
      const Timestep& timestep) const override;
  void Aggregate(const Timestep& timestep,
                 absl::Span<std::unique_ptr<HazardHistogramObserver> const>
                     observers) override;

 private:
  LinearHistogram<float, internal::kHazardHistogramBuckets>
      cumulative_histogram_;
  std::unique_ptr<file::FileWriter> writer_;
};

}  // namespace abesim

#endif  // AGENT_BASED_EPIDEMIC_SIM_APPLICATIONS_RISK_LEARNING_OBSERVERS_H_
