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

#include <memory>
#include <utility>

#include "absl/container/flat_hash_map.h"
#include "absl/random/distributions.h"
#include "absl/random/random.h"
#include "absl/time/time.h"
#include "agent_based_epidemic_sim/core/event.h"
#include "agent_based_epidemic_sim/core/exposure_generator.h"
#include "agent_based_epidemic_sim/core/micro_exposure_generator_builder.h"

namespace abesim {

namespace {

class GraphLocation : public Location {
 public:
  GraphLocation(int64 uuid, float drop_probability,
                std::vector<std::pair<int64, int64>> graph,
                absl::Duration visit_length_mean,
                absl::Duration visit_length_stddev,
                std::unique_ptr<ExposureGenerator> exposure_generator)
      : uuid_(uuid),
        drop_probability_(drop_probability),
        graph_(std::move(graph)),
        visit_length_mean_(visit_length_mean),
        visit_length_stddev_(visit_length_stddev),
        exposure_generator_(std::move(exposure_generator)) {}

  int64 uuid() const override { return uuid_; }

  void ProcessVisits(absl::Span<const Visit> visits,
                     Broker<InfectionOutcome>* infection_broker) override {
    thread_local absl::flat_hash_map<int64, float> infectivity;
    thread_local absl::flat_hash_map<int64, float> symptom_factor;
    infectivity.clear();
    for (const Visit& visit : visits) {
      infectivity[visit.agent_uuid] = visit.infectivity;
      symptom_factor[visit.agent_uuid] = visit.symptom_factor;
    }

    for (const std::pair<int64, int64>& edge : graph_) {
      // Randomly drop some potential contacts.
      if (absl::Bernoulli(gen_, drop_probability_)) continue;

      // If either of the participants are not present, no contact is generated.
      auto infectivity_a = infectivity.find(edge.first);
      if (infectivity_a == infectivity.end()) continue;
      auto infectivity_b = infectivity.find(edge.second);
      if (infectivity_b == infectivity.end()) continue;
      auto symptom_factor_a = symptom_factor.find(edge.first);
      if (symptom_factor_a == symptom_factor.end()) continue;
      auto symptom_factor_b = symptom_factor.find(edge.second);
      if (symptom_factor_b == symptom_factor.end()) continue;

      HostData host_a = {.start_time = absl::UnixEpoch(),
                         .infectivity = infectivity_a->second,
                         .symptom_factor = symptom_factor_a->second};
      HostData host_b = {.start_time = absl::UnixEpoch(),
                         .infectivity = infectivity_b->second,
                         .symptom_factor = symptom_factor_b->second};

      ExposurePair host_exposures =
          exposure_generator_->Generate(host_a, host_b);

      infection_broker->Send(
          {{
               .agent_uuid = edge.first,
               .exposure = host_exposures.host_a,
               .exposure_type = InfectionOutcomeProto::CONTACT,
               .source_uuid = edge.second,
           },
           {
               .agent_uuid = edge.second,
               .exposure = host_exposures.host_b,
               .exposure_type = InfectionOutcomeProto::CONTACT,
               .source_uuid = edge.first,
           }});
    }
  }

 private:
  const int64 uuid_;
  const float drop_probability_;
  const std::vector<std::pair<int64, int64>> graph_;
  const absl::Duration visit_length_mean_;
  const absl::Duration visit_length_stddev_;
  std::unique_ptr<ExposureGenerator> exposure_generator_;
  absl::BitGen gen_;
};

}  // namespace

std::unique_ptr<Location> NewGraphLocation(
    int64 uuid, float drop_probability,
    std::vector<std::pair<int64, int64>> graph,
    absl::Duration visit_length_mean, absl::Duration visit_length_stddev,
    std::unique_ptr<ExposureGenerator> exposure_generator) {
  return absl::make_unique<GraphLocation>(
      uuid, drop_probability, std::move(graph), visit_length_mean,
      visit_length_stddev, std::move(exposure_generator));
}

}  // namespace abesim
