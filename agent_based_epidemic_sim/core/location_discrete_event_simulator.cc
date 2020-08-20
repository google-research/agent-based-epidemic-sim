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

#include <memory>
#include <queue>

#include "absl/random/random.h"
#include "absl/time/time.h"
#include "agent_based_epidemic_sim/core/exposure_generator.h"
#include "agent_based_epidemic_sim/port/logging.h"

namespace abesim {
namespace {

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

void RecordContact(VisitNode* a, VisitNode* b,
                   ExposureGenerator* exposure_generator) {
  // TODO: Incorporate a notion of guaranteed exposure duration
  // into this method.

  HostData host_a = {.infectivity = a->visit->infectivity,
                     .symptom_factor = a->visit->symptom_factor};
  HostData host_b = {.infectivity = b->visit->infectivity,
                     .symptom_factor = b->visit->symptom_factor};

  // TODO: Generate multiple ExposurePairs when overlap exceeds
  // current Exposure duration cap. There is currently a cap to the Exposure
  // duration because of the fixed size of ProximityTrace.
  ExposurePair host_exposures = exposure_generator->Generate(host_a, host_b);

  a->contacts.push_back({.other_uuid = b->visit->agent_uuid,
                         .other_state = b->visit->health_state,
                         .exposure = host_exposures.host_a});
  b->contacts.push_back({.other_uuid = a->visit->agent_uuid,
                         .other_state = a->visit->health_state,
                         .exposure = host_exposures.host_b});
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
        RecordContact(event.node, node, exposure_generator_.get());
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
