#include "agent_based_epidemic_sim/applications/home_work/learning_contacts_observer.h"

#include "absl/strings/str_cat.h"
#include "absl/strings/substitute.h"
#include "absl/types/span.h"
#include "agent_based_epidemic_sim/core/agent.h"
#include "agent_based_epidemic_sim/core/event.h"
#include "agent_based_epidemic_sim/core/health_state.h"
#include "agent_based_epidemic_sim/core/timestep.h"
#include "agent_based_epidemic_sim/port/file_utils.h"

namespace abesim {

LearningContactsObserver::LearningContactsObserver() {}

void LearningContactsObserver::Observe(
    const Agent& agent, absl::Span<const InfectionOutcome> outcomes) {
  for (const auto& outcome : outcomes) {
    // We only care about CONTACT exposures for learning output.
    if (outcome.exposure_type != InfectionOutcomeProto::CONTACT) continue;
    // For our current purposes, we do not care about contacts between two
    // healthy agents or two infectious agents. Also non-infective exposures are
    // excluded.
    bool agent_infectious = IsInfectious(agent.CurrentHealthState());
    bool contact_infectious = outcome.exposure.infectivity > 0;
    if (agent_infectious != contact_infectious) continue;
    outcomes_.push_back(outcome);
  }
}

LearningContactsObserverFactory::LearningContactsObserverFactory(
    absl::string_view output_pattern)
    : output_pattern_(output_pattern) {}

void LearningContactsObserverFactory::Aggregate(
    const Timestep& timestep,
    absl::Span<std::unique_ptr<LearningContactsObserver> const> observers) {
  const std::string csv_path_base = absl::StrCat(
      output_pattern_, "_", absl::FormatTime(timestep.start_time()));
  auto contacts_writer =
      file::OpenOrDie(absl::StrCat(csv_path_base, "_contacts.csv"));
  status_.Update(contacts_writer->WriteString(
      "source_uuid,sink_uuid,start_time,duration,location,infectivity\n"));
  for (const auto& observer : observers) {
    for (const auto& outcome : observer->outcomes_) {
      const std::string contact_line = absl::Substitute(
          "$0,$1,$2,$3,$4,$5\n", outcome.source_uuid, outcome.agent_uuid,
          absl::FormatTime(outcome.exposure.start_time),
          absl::FormatDuration(outcome.exposure.duration), "unknown",
          outcome.exposure.infectivity);
      status_.Update(contacts_writer->WriteString(contact_line));
    }
  }
}

std::unique_ptr<LearningContactsObserver>
LearningContactsObserverFactory::MakeObserver(const Timestep& timestep) const {
  return absl::make_unique<LearningContactsObserver>();
}

}  // namespace abesim
