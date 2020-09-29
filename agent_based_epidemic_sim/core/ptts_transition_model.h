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

#ifndef AGENT_BASED_EPIDEMIC_SIM_CORE_PTTS_TRANSITION_MODEL_H_
#define AGENT_BASED_EPIDEMIC_SIM_CORE_PTTS_TRANSITION_MODEL_H_

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
  static std::unique_ptr<TransitionModel> CreateFromProto(
      const PTTSTransitionModelProto& proto);

  PTTSTransitionModel(const PTTSTransitionModel&) = delete;
  PTTSTransitionModel& operator=(const PTTSTransitionModel&) = delete;

  HealthTransition GetNextHealthTransition(
      const HealthTransition& latest_transition) override;

 private:
  struct Edge {
    HealthState::State src;
    HealthState::State dst;
    float weight = 0.0;
    std::gamma_distribution<float> days;
  };
  PTTSTransitionModel(std::vector<Edge> edges);
  std::vector<Edge> edges_;
};

}  // namespace abesim

#endif  // AGENT_BASED_EPIDEMIC_SIM_CORE_PTTS_TRANSITION_MODEL_H_
