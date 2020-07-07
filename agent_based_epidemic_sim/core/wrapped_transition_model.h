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

#ifndef AGENT_BASED_EPIDEMIC_SIM_CORE_WRAPPED_TRANSITION_MODEL_H_
#define AGENT_BASED_EPIDEMIC_SIM_CORE_WRAPPED_TRANSITION_MODEL_H_

#include "agent_based_epidemic_sim/core/event.h"
#include "agent_based_epidemic_sim/core/transition_model.h"
#include "agent_based_epidemic_sim/core/visit.h"

namespace abesim {

// Models transition between health states for a given disease.
class WrappedTransitionModel : public TransitionModel {
 public:
  explicit WrappedTransitionModel(TransitionModel* transition_model)
      : transition_model_(transition_model) {}

  // Computes the next state transition given the current state and transition
  // time.
  HealthTransition GetNextHealthTransition(
      const HealthTransition& latest_transition) override {
    return transition_model_->GetNextHealthTransition(latest_transition);
  }

 private:
  // Unowned (must outlive this class).
  TransitionModel* transition_model_;
};

}  // namespace abesim

#endif  // AGENT_BASED_EPIDEMIC_SIM_CORE_WRAPPED_TRANSITION_MODEL_H_
