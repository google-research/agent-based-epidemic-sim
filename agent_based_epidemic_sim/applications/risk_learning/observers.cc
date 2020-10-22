#include "agent_based_epidemic_sim/applications/risk_learning/observers.h"

#include "absl/memory/memory.h"
#include "absl/strings/str_format.h"
#include "absl/time/time.h"
#include "agent_based_epidemic_sim/applications/risk_learning/exposures_per_test_result.pb.h"
#include "agent_based_epidemic_sim/port/time_proto_util.h"
#include "agent_based_epidemic_sim/util/records.h"

namespace abesim {

SummaryObserver::SummaryObserver(const Timestep timestep)
    : timestep_(timestep), counts_({}) {}

void SummaryObserver::Observe(const Agent& agent,
                              absl::Span<const InfectionOutcome>) {
  counts_[agent.CurrentHealthState()]++;
}

SummaryObserverFactory::SummaryObserverFactory(
    absl::string_view summary_filename)
    : writer_(file::OpenOrDie(summary_filename)) {
  std::string header = "DATE";
  for (const HealthState::State state : kOutputStates) {
    header += ", " + HealthState::State_Name(state);
  }
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
  for (const auto& observer : observers) {
    for (HealthState::State state : kOutputStates) {
      counts[state] += observer->counts_[state];
    }
  }

  std::string line =
      absl::FormatTime("%Y-%m-%d", timestep.start_time(), absl::UTCTimeZone());
  for (HealthState::State state : kOutputStates) {
    absl::StrAppendFormat(&line, ", %d", counts[state]);
  }
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

LearningObserverFactory::LearningObserverFactory(
    absl::string_view learning_filename)
    : writer_(MakeRecordWriter(learning_filename)) {}

LearningObserverFactory::~LearningObserverFactory() { writer_.Close(); }

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
