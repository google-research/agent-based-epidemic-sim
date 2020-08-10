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
      auto first_infectivity = infectivity.find(edge.first);
      if (first_infectivity == infectivity.end()) continue;
      auto second_infectivity = infectivity.find(edge.second);
      if (second_infectivity == infectivity.end()) continue;
      auto first_symptom_factor = symptom_factor.find(edge.first);
      if (first_symptom_factor == symptom_factor.end()) continue;
      auto second_symptom_factor = symptom_factor.find(edge.second);
      if (second_symptom_factor == symptom_factor.end()) continue;

      // If two agents are connected by an edge, we  randomly generate a
      // duration and the corresponding micro exposures that result.
      const float mean = absl::FDivDuration(visit_length_mean_, absl::Hours(1));
      const float stdev =
          absl::FDivDuration(visit_length_stddev_, absl::Hours(1));
      const absl::Duration overlap =
          absl::Hours(absl::Gaussian(gen_, mean, stdev));

      infection_broker->Send(
          {{
               .agent_uuid = edge.first,
               .exposure = exposure_generator_->Generate(
                   absl::UnixEpoch(), overlap, second_infectivity->second,
                   second_symptom_factor->second),
               .exposure_type = InfectionOutcomeProto::CONTACT,
               .source_uuid = edge.second,
           },
           {
               .agent_uuid = edge.second,
               .exposure = exposure_generator_->Generate(
                   absl::UnixEpoch(), overlap, first_infectivity->second,
                   first_symptom_factor->first),
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
