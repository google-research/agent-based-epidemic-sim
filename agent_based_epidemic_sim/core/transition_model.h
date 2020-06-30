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

#ifndef THIRD_PARTY_AGENT_BASED_EPIDEMIC_SIM_CORE_TRANSITION_MODEL_H_
#define THIRD_PARTY_AGENT_BASED_EPIDEMIC_SIM_CORE_TRANSITION_MODEL_H_

#include "agent_based_epidemic_sim/core/event.h"
#include "agent_based_epidemic_sim/core/visit.h"

namespace abesim {

// Models transition between health states for a given disease.
class TransitionModel {
 public:
  // Computes the next state transition given the current state and transition
  // time.
  virtual HealthTransition GetNextHealthTransition(
      const HealthTransition& latest_transition) = 0;
  virtual ~TransitionModel() = default;
};

}  // namespace abesim

#endif  // THIRD_PARTY_AGENT_BASED_EPIDEMIC_SIM_CORE_TRANSITION_MODEL_H_
