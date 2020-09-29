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

#include "agent_based_epidemic_sim/core/ptts_transition_model.h"

#include <algorithm>
#include <random>

#include "absl/memory/memory.h"
#include "absl/meta/type_traits.h"
#include "absl/random/distributions.h"
#include "absl/time/time.h"
#include "agent_based_epidemic_sim/core/enum_indexed_array.h"
#include "agent_based_epidemic_sim/core/event.h"
#include "agent_based_epidemic_sim/core/pandemic.pb.h"
#include "agent_based_epidemic_sim/core/ptts_transition_model.pb.h"
#include "agent_based_epidemic_sim/core/random.h"

namespace abesim {

PTTSTransitionModel::PTTSTransitionModel(std::vector<Edge> edges)
    : edges_(std::move(edges)) {
  std::sort(edges_.begin(), edges_.end(), [](const Edge& a, const Edge& b) {
    if (a.src != b.src) return a.src < b.src;
    return a.weight < b.weight;
  });
  // Normalize the edge weights.
  EnumIndexedArray<float, HealthState::State, HealthState::State_ARRAYSIZE> w{};
  for (const Edge& e : edges_) {
    DCHECK_GT(e.weight, 0.0) << "Zero weight edge in PTTSTransition model.";
    w[e.src] += e.weight;
  }
  for (Edge& e : edges_) {
    e.weight /= w[e.src];
  }
}

/* static */
std::unique_ptr<TransitionModel> PTTSTransitionModel::CreateFromProto(
    const PTTSTransitionModelProto& proto) {
  std::vector<Edge> edges;

  for (const PTTSTransitionModelProto::TransitionProbabilities& src :
       proto.state_transition_diagram()) {
    for (const PTTSTransitionModelProto::TransitionProbability& dst :
         src.transition_probability()) {
      const float b = dst.sd_days_to_transition() *
                      dst.sd_days_to_transition() /
                      dst.mean_days_to_transition();
      const float a = dst.mean_days_to_transition() / b;
      edges.push_back({.src = src.health_state(),
                       .dst = dst.health_state(),
                       .weight = dst.transition_probability(),
                       .days = std::gamma_distribution<float>(a, b)});
    }
  }
  return absl::WrapUnique(new PTTSTransitionModel(std::move(edges)));
}

HealthTransition PTTSTransitionModel::GetNextHealthTransition(
    const HealthTransition& latest_transition) {
  absl::BitGenRef gen = GetBitGen();
  auto edge = std::lower_bound(
      edges_.begin(), edges_.end(), latest_transition.health_state,
      [](const Edge& e, const HealthState::State& s) { return e.src < s; });
  if (edge == edges_.end() || edge->src != latest_transition.health_state) {
    // There is no transition out of the current state, so we will return a
    // self transition that occurs in the infinite future.
    return {
        .time = absl::InfiniteFuture(),
        .health_state = latest_transition.health_state,
    };
  }
  // The maximum number of edges out of any state is 3, so a linear search here
  // is fine, especially since we put the highest weight edges first.
  float s = absl::Uniform(gen, 0.0, 1.0);
  while (true) {
    s -= edge->weight;
    if (s <= 0.0) break;
    edge++;
  }

  return {
      .time = latest_transition.time + absl::Hours(24 * edge->days(gen)),
      .health_state = edge->dst,
  };
}

}  // namespace abesim
