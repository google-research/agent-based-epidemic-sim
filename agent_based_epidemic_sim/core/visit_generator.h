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

#ifndef THIRD_PARTY_AGENT_BASED_EPIDEMIC_SIM_CORE_VISIT_GENERATOR_H_
#define THIRD_PARTY_AGENT_BASED_EPIDEMIC_SIM_CORE_VISIT_GENERATOR_H_

#include "agent_based_epidemic_sim/core/event.h"
#include "agent_based_epidemic_sim/core/public_policy.h"
#include "agent_based_epidemic_sim/core/timestep.h"
#include "agent_based_epidemic_sim/core/visit.h"

namespace abesim {

// Generates visits to locations.
class VisitGenerator {
 public:
  virtual void GenerateVisits(const Timestep& timestep,
                              const PublicPolicy* policy,
                              HealthState::State current_health_state,
                              const ContactSummary& contact_summary,
                              std::vector<Visit>* visits) = 0;
  virtual ~VisitGenerator() = default;
};

}  // namespace abesim

#endif  // THIRD_PARTY_AGENT_BASED_EPIDEMIC_SIM_CORE_VISIT_GENERATOR_H_
