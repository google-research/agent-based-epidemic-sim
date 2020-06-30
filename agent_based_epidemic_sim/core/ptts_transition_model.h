/*
 * Copyright 2020 Google LLC
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#ifndef THIRD_PARTY_AGENT_BASED_EPIDEMIC_SIM_CORE_PTTS_TRANSITION_MODEL_H_
#define THIRD_PARTY_AGENT_BASED_EPIDEMIC_SIM_CORE_PTTS_TRANSITION_MODEL_H_

#include "absl/container/flat_hash_map.h"
#include "absl/random/discrete_distribution.h"
#include "absl/random/random.h"
#include "agent_based_epidemic_sim/core/enum_indexed_array.h"
#include "agent_based_epidemic_sim/core/event.h"
#include "agent_based_epidemic_sim/core/ptts_transition_model.pb.h"
#include "agent_based_epidemic_sim/core/transition_model.h"
#include "agent_based_epidemic_sim/core/visit.h"

namespace abesim {

// Models transition between health states for a given disease by implementing
// a probabilistic timed transition model (PTTS) finite state machine.
// Specifically, the dynamics are modeled by a continuous-time Markov chain (or
// Markov process). At each call to GetNextHealthTransition, the model samples
// from an exponential distribution to determine the dwell time in the current
// state (contained in the latest_transition), and samples from the discrete
// distribution of transitions to determine the state transition at the end of
// the dwell time.
//
// The model is stateful in the sense of the distribution sampling: the
// BitGen is mutated when sampling from both distributions.
// Thread safety is the responsibility of callers.
class PTTSTransitionModel : public TransitionModel {
 public:
  struct TransitionProbabilities {
    // The transition probabilities for the next states.
    absl::discrete_distribution<int> transitions;
    // The rate parameter of the exponential distribution modeling dwell time in
    // the current state.
    float rate = 0.0f;
  };
  using StateTransitionDiagram =
      EnumIndexedArray<TransitionProbabilities, HealthState::State,
                       HealthState::State_ARRAYSIZE>;

  static std::unique_ptr<TransitionModel> CreateFromProto(
      const PTTSTransitionModelProto& proto);

  explicit PTTSTransitionModel(
      const StateTransitionDiagram& state_transition_diagram)
      : state_transition_diagram_(state_transition_diagram) {}

  PTTSTransitionModel(const PTTSTransitionModel&) = delete;
  PTTSTransitionModel& operator=(const PTTSTransitionModel&) = delete;

  HealthTransition GetNextHealthTransition(
      const HealthTransition& latest_transition) override;

 private:
  // State transition model and probabilities.
  // TODO: Consider wrapping transition_probabilities_ and transitions_
  // in an object so their behavior (distribution sequence) can be mocked.
  // absl::discrete_distribution does not support mocks/mocked sequences.
  // Possibly, this should be a unique_ptr per owner -- need to consider the
  // significance of multiple sequences being generated from the same
  // distribution sampler.
  StateTransitionDiagram state_transition_diagram_;
  absl::BitGen gen_;
};

}  // namespace abesim

#endif  // THIRD_PARTY_AGENT_BASED_EPIDEMIC_SIM_CORE_PTTS_TRANSITION_MODEL_H_
