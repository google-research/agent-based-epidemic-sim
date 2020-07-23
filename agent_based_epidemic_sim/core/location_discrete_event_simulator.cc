// Copyright 2020 Google LLC
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//      http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#include "agent_based_epidemic_sim/core/location_discrete_event_simulator.h"

#include <queue>

#include "absl/random/random.h"
#include "absl/time/time.h"
#include "agent_based_epidemic_sim/port/logging.h"

namespace abesim {
namespace {

// TODO: Move  into an event message about visiting infectious agents.
constexpr float kInfectivity = 1;

// Corresponds to the record of a visiting agent in a location.
struct VisitNode {
  const Visit* visit;
  std::list<VisitNode*>::iterator pos;
  std::vector<Contact> contacts;

  VisitNode() = default;
  explicit VisitNode(const Visit* node_visit) : visit(node_visit) {}
};

// The type of event.
enum class EventType { ARRIVAL = 0, DEPARTURE = 1 };

// An event corresponding to an arrival or departure of an individual with some
// state of health.
struct Event {
  EventType type;
  VisitNode* node;
};

// Sorts the event list by time in ascending order.
bool IsEventEarlier(const Event& a, const Event& b) {
  auto event_time_fn = [](const Event& event) {
    return event.type == EventType::ARRIVAL ? event.node->visit->start_time
                                            : event.node->visit->end_time;
  };
  return event_time_fn(a) < event_time_fn(b);
}

void ConvertVisitsToEvents(
    const absl::Span<const Visit> visits, std::vector<Event>* events,
    std::vector<std::unique_ptr<VisitNode>>* visit_nodes) {
  for (const Visit& visit : visits) {
    if (visit.start_time >= visit.end_time) {
      LOG(DFATAL) << "Skipping visit end_time <= start_time: " << visit;
      continue;
    }
    visit_nodes->push_back(absl::make_unique<VisitNode>(&visit));
    events->push_back(
        {.type = EventType::ARRIVAL, .node = visit_nodes->back().get()});
    events->push_back(
        {.type = EventType::DEPARTURE, .node = visit_nodes->back().get()});
  }
}

absl::Duration Overlap(const Visit& a, const Visit& b) {
  return std::min(a.end_time, b.end_time) -
         std::max(a.start_time, b.start_time);
}

const std::array<uint8, kNumberMicroExposureBuckets> GenerateMicroExposures(
    absl::Duration overlap) {
  std::array<uint8, kNumberMicroExposureBuckets> micro_exposure_counts = {};

  // TODO: Use a distribution of duration@distance once it is
  // figured out.
  // Generate counts for each bucket and never over assign
  // duration.
  const uint8 total_counts_to_assign = absl::ToInt64Minutes(overlap);

  if (total_counts_to_assign == 0) return micro_exposure_counts;

  const uint8 buckets_to_fill =
      std::min(kNumberMicroExposureBuckets, total_counts_to_assign);
  const uint8 counts_per_bucket = total_counts_to_assign / buckets_to_fill;

  for (auto i = 0; i < buckets_to_fill; i++) {
    micro_exposure_counts[i] = counts_per_bucket;
  }

  return micro_exposure_counts;
}

void RecordContact(VisitNode* a, VisitNode* b) {
  const absl::Duration overlap = Overlap(*a->visit, *b->visit);
  const std::array<uint8, kNumberMicroExposureBuckets> micro_exposure_counts =
      GenerateMicroExposures(overlap);

  a->contacts.push_back(
      {.other_uuid = b->visit->agent_uuid,
       .other_state = b->visit->health_state,
       .exposure = {.duration = overlap,
                    .micro_exposure_counts = micro_exposure_counts,
                    .infectivity = b->visit->infectivity,
                    .symptom_factor = b->visit->symptom_factor}});
  b->contacts.push_back(
      {.other_uuid = a->visit->agent_uuid,
       .other_state = a->visit->health_state,
       .exposure = {.duration = overlap,
                    .micro_exposure_counts = micro_exposure_counts,
                    .infectivity = a->visit->infectivity,
                    .symptom_factor = a->visit->symptom_factor}});
}
}  // namespace

void LocationDiscreteEventSimulator::ProcessVisits(
    const absl::Span<const Visit> visits,
    Broker<InfectionOutcome>* infection_broker) {
  auto matches_uuid_fn = [this](const absl::Span<const Visit> visits) {
    return std::all_of(visits.begin(), visits.end(),
                       [this](const Visit& visit) {
                         if (visit.location_uuid != uuid()) {
                           LOG(WARNING) << "Visit uuid: " << visit.agent_uuid
                                        << " expected: " << uuid();
                           return false;
                         }
                         return true;
                       });
  };
  DCHECK(matches_uuid_fn(visits)) << "Found incorrect Visit uuid.";
  thread_local std::vector<Event> events(2 * visits.size());
  events.clear();
  thread_local std::vector<std::unique_ptr<VisitNode>> visit_nodes(
      visits.size());
  visit_nodes.clear();
  ConvertVisitsToEvents(visits, &events, &visit_nodes);
  std::sort(events.begin(), events.end(), IsEventEarlier);
  std::list<VisitNode*> active_visits;
  for (Event& event : events) {
    if (event.type == EventType::ARRIVAL) {
      for (VisitNode* node : active_visits) {
        RecordContact(event.node, node);
      }
      event.node->pos = active_visits.insert(active_visits.end(), event.node);
    } else {
      std::vector<InfectionOutcome> infection_outcomes;
      infection_outcomes.reserve(event.node->contacts.size());
      for (const Contact& contact : event.node->contacts) {
        infection_outcomes.push_back(
            {.agent_uuid = event.node->visit->agent_uuid,
             .exposure = contact.exposure,
             .exposure_type = InfectionOutcomeProto::CONTACT,
             .source_uuid = contact.other_uuid});
      }
      infection_broker->Send(infection_outcomes);
      active_visits.erase(event.node->pos);
    }
  }
}

}  // namespace abesim
