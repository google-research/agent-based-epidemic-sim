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

#include "agent_based_epidemic_sim/core/graph_location.h"

#include "absl/container/flat_hash_map.h"
#include "absl/random/distributions.h"
#include "absl/random/random.h"
#include "agent_based_epidemic_sim/core/event.h"

namespace abesim {

namespace {

class GraphLocation : public Location {
 public:
  GraphLocation(int64 uuid, float drop_probability,
                std::vector<std::pair<int64, int64>> graph)
      : uuid_(uuid),
        drop_probability_(drop_probability),
        graph_(std::move(graph)) {}

  int64 uuid() const override { return uuid_; }

  void ProcessVisits(absl::Span<const Visit> visits,
                     Broker<InfectionOutcome>* infection_broker) override {
    thread_local absl::flat_hash_map<int64, float> infectivity;
    infectivity.clear();
    for (const Visit& visit : visits) {
      infectivity[visit.agent_uuid] =
          visit.health_state == HealthState::INFECTIOUS ? 1.0 : 0.0;
    }

    for (const std::pair<int64, int64>& edge : graph_) {
      // Randomly drop some potential contacts.
      if (absl::Bernoulli(gen_, drop_probability_)) continue;

      // If either of the participants are not present, no contact is generated.
      auto first_infectivity = infectivity.find(edge.first);
      if (first_infectivity == infectivity.end()) continue;
      auto second_infectivity = infectivity.find(edge.second);
      if (second_infectivity == infectivity.end()) continue;

      // Note that we do not report times or durations.  A visit either occurs
      // on a given day or not.
      infection_broker->Send(
          {{
               .agent_uuid = edge.first,
               .exposure =
                   {
                       .infectivity = second_infectivity->second,
                   },
               .exposure_type = InfectionOutcomeProto::CONTACT,
               .source_uuid = edge.second,
           },
           {
               .agent_uuid = edge.second,
               .exposure =
                   {
                       .infectivity = first_infectivity->second,
                   },
               .exposure_type = InfectionOutcomeProto::CONTACT,
               .source_uuid = edge.first,
           }});
    }
  }

 private:
  const int64 uuid_;
  const float drop_probability_;
  const std::vector<std::pair<int64, int64>> graph_;
  absl::BitGen gen_;
};

}  // namespace

std::unique_ptr<Location> NewGraphLocation(
    int64 uuid, float drop_probability,
    std::vector<std::pair<int64, int64>> graph) {
  return absl::make_unique<GraphLocation>(uuid, drop_probability,
                                          std::move(graph));
}

}  // namespace abesim
