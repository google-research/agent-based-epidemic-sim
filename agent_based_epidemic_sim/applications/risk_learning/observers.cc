#include "agent_based_epidemic_sim/applications/risk_learning/observers.h"

#include "absl/memory/memory.h"
#include "absl/strings/str_format.h"
#include "absl/time/time.h"
#include "agent_based_epidemic_sim/applications/risk_learning/exposures_per_test_result.pb.h"
#include "agent_based_epidemic_sim/port/time_proto_util.h"
#include "agent_based_epidemic_sim/util/records.h"

namespace abesim {
namespace {
constexpr char kNewlySymptomaticMild[] = "NEWLY_SYMPTOMATIC_MILD";
constexpr char kNewlySymptomaticSevere[] = "NEWLY_SYMPTOMATIC_SEVERE";
constexpr char kNewlyTestPositive[] = "NEWLY_TEST_POSITIVE";
}  // namespace

SummaryObserver::SummaryObserver(const Timestep timestep)
    : timestep_(timestep), counts_({}) {}

void SummaryObserver::Observe(const Agent& agent,
                              absl::Span<const InfectionOutcome>) {
  counts_[agent.CurrentHealthState()]++;
  if (agent.HealthTransitions().back().time >= timestep_.start_time()) {
    if (agent.CurrentHealthState() == HealthState::SYMPTOMATIC_MILD) {
      newly_symptomatic_mild_++;
    } else if (agent.CurrentHealthState() == HealthState::SYMPTOMATIC_SEVERE) {
      newly_symptomatic_severe_++;
    }
  }
  if (agent.CurrentTestResult(timestep_).time_received >=
          timestep_.start_time() &&
      agent.CurrentTestResult(timestep_).outcome == TestOutcome::POSITIVE) {
    newly_test_positive_++;
  }
}

SummaryObserverFactory::SummaryObserverFactory(
    absl::string_view summary_filename)
    : writer_(file::OpenOrDie(summary_filename)) {
  std::string header = "DATE";
  for (const HealthState::State state : kOutputStates) {
    header += ", " + HealthState::State_Name(state);
  }
  absl::StrAppendFormat(&header, ", %s", kNewlySymptomaticMild);
  absl::StrAppendFormat(&header, ", %s", kNewlySymptomaticSevere);
  absl::StrAppendFormat(&header, ", %s", kNewlyTestPositive);
  header += "\n";
  absl::Status status = writer_->WriteString(header);
  if (!status.ok()) LOG(ERROR) << status;
}

SummaryObserverFactory::~SummaryObserverFactory() {
  absl::Status status = writer_->Close();
  if (!status.ok()) LOG(ERROR) << status;
}

std::unique_ptr<SummaryObserver> SummaryObserverFactory::MakeObserver(
    const Timestep& timestep) const {
  return absl::make_unique<SummaryObserver>(timestep);
}

void SummaryObserverFactory::Aggregate(
    const Timestep& timestep,
    absl::Span<std::unique_ptr<SummaryObserver> const> observers) {
  HealthStateCounts counts = {};
  int newly_test_positive = 0;
  int newly_symptomatic_mild = 0;
  int newly_symptomatic_severe = 0;
  for (const auto& observer : observers) {
    for (HealthState::State state : kOutputStates) {
      counts[state] += observer->counts_[state];
    }
    newly_symptomatic_mild += observer->newly_symptomatic_mild_;
    newly_symptomatic_severe += observer->newly_symptomatic_severe_;
    newly_test_positive += observer->newly_test_positive_;
  }

  std::string line =
      absl::FormatTime("%Y-%m-%d", timestep.start_time(), absl::UTCTimeZone());
  for (HealthState::State state : kOutputStates) {
    absl::StrAppendFormat(&line, ", %d", counts[state]);
  }
  absl::StrAppendFormat(&line, ", %d", newly_symptomatic_mild);
  absl::StrAppendFormat(&line, ", %d", newly_symptomatic_severe);
  absl::StrAppendFormat(&line, ", %d", newly_test_positive);
  line += "\n";
  absl::Status status = writer_->WriteString(line);
  if (!status.ok()) LOG(ERROR) << status;
}

LearningObserver::LearningObserver(Timestep timestep) : timestep_(timestep) {}

template <typename T, typename P>
void ToProto(T src, P* result) {
  absl::Status status = EncodeGoogleApiProto(src, result);
  if (!status.ok()) LOG(ERROR) << status;
}

void LearningObserver::Observe(const Agent& agent,
                               absl::Span<const InfectionOutcome>) {
  TestResult test = agent.CurrentTestResult(timestep_);
  // We report results on the day test results become available.
  if (test.time_received == absl::InfiniteFuture() ||
      test.time_received < timestep_.start_time() ||
      test.time_received >= timestep_.end_time())
    return;

  results_.emplace_back();
  ExposuresPerTestResult::ExposureResult& result = results_.back();

  result.set_agent_uuid(agent.uuid());
  result.set_outcome(test.outcome);
  result.set_hazard(test.hazard);
  ToProto(test.time_requested, result.mutable_test_administered_time());
  ToProto(test.time_received, result.mutable_test_received_time());
  std::optional<absl::Time> infection_onset = agent.infection_onset();
  if (infection_onset.has_value()) {
    ToProto(*infection_onset, result.mutable_infection_onset_time());
  }

  const ExposureStore* exposures = agent.exposure_store();
  if (exposures == nullptr) return;
  exposures->PerExposure(
      absl::InfinitePast(), [&result](int64 uuid, const Exposure& exposure,
                                      const ContactReport* report) {
        auto e = result.add_exposures();
        ToProto(exposure.start_time, e->mutable_exposure_time());
        e->set_exposure_type(report != nullptr
                                 ? ExposuresPerTestResult::CONFIRMED
                                 : ExposuresPerTestResult::UNCONFIRMED);
        e->set_source_uuid(uuid);
        ToProto(exposure.duration, e->mutable_duration());
        e->set_distance(exposure.distance);

        if (report != nullptr &&
            report->initial_symptom_onset_time.has_value()) {
          ToProto(exposure.start_time - *report->initial_symptom_onset_time,
                  e->mutable_duration_since_symptom_onset());
        }
      });
}

// Note: num_workers is reduced by 1 since 1 means no parallelism in the
// risk_learning application, while 0 means no parallelism for the RecordWriter.
LearningObserverFactory::LearningObserverFactory(
    absl::string_view learning_filename, const int num_workers)
    : writer_(MakeRecordWriter(learning_filename, num_workers - 1)) {}

LearningObserverFactory::~LearningObserverFactory() {
  if (!writer_.Close()) LOG(ERROR) << writer_.status();
}

std::unique_ptr<LearningObserver> LearningObserverFactory::MakeObserver(
    const Timestep& timestep) const {
  return absl::make_unique<LearningObserver>(timestep);
}

void LearningObserverFactory::Aggregate(
    const Timestep& timestep,
    absl::Span<std::unique_ptr<LearningObserver> const> observers) {
  for (const auto& observer : observers) {
    for (const auto& result : observer->results_) {
      writer_.WriteRecord(result);
    }
  }
}

}  // namespace abesim
