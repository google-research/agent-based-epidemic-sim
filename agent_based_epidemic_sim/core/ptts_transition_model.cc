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

#include "absl/time/time.h"
#include "agent_based_epidemic_sim/core/event.h"

namespace pandemic {
namespace {

PTTSTransitionModel::TransitionProbabilities FromProto(
    const PTTSTransitionModelProto::TransitionProbabilities& proto) {
  EnumIndexedArray<float, HealthState::State, HealthState::State_ARRAYSIZE>
      indexed_transitions{};
  for (const auto& transition_probability : proto.transition_probability()) {
    indexed_transitions[transition_probability.health_state()] =
        transition_probability.transition_probability();
  }
  return {.transitions = absl::discrete_distribution<int>(
              indexed_transitions.begin(), indexed_transitions.end()),
          .rate = proto.rate()};
}
}  // namespace

/* static */
std::unique_ptr<TransitionModel> PTTSTransitionModel::CreateFromProto(
    const PTTSTransitionModelProto& proto) {
  PTTSTransitionModel::StateTransitionDiagram state_transition_diagram;

  for (const auto& transition_probabilities :
       proto.state_transition_diagram()) {
    state_transition_diagram[transition_probabilities.health_state()] =
        FromProto(transition_probabilities);
  }
  return absl::make_unique<PTTSTransitionModel>(state_transition_diagram);
}

HealthTransition PTTSTransitionModel::GetNextHealthTransition(
    const HealthTransition& latest_transition) {
  absl::Duration dwell_time = absl::Hours(
      24 * absl::Exponential(
               gen_,
               state_transition_diagram_[latest_transition.health_state].rate));
  HealthTransition next_transition;
  next_transition.health_state = HealthState::State(
      state_transition_diagram_[latest_transition.health_state].transitions(
          gen_));
  next_transition.time = latest_transition.time + dwell_time;
  return next_transition;
}

}  // namespace pandemic
