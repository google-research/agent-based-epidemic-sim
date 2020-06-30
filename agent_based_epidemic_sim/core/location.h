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

#ifndef THIRD_PARTY_AGENT_BASED_EPIDEMIC_SIM_CORE_LOCATION_H_
#define THIRD_PARTY_AGENT_BASED_EPIDEMIC_SIM_CORE_LOCATION_H_

#include "absl/types/span.h"
#include "agent_based_epidemic_sim/core/broker.h"
#include "agent_based_epidemic_sim/core/observer.h"
#include "agent_based_epidemic_sim/core/visit.h"

namespace abesim {

// Simulates a location during a timestep.
class Location {
 public:
  virtual int64 uuid() const = 0;

  // Process a set of visits and write InfectionOutcomes to the given
  // infection_broker.
  virtual void ProcessVisits(absl::Span<const Visit> visits,
                             Broker<InfectionOutcome>* infection_broker) = 0;

  virtual ~Location() = default;
};

}  // namespace abesim

#endif  // THIRD_PARTY_AGENT_BASED_EPIDEMIC_SIM_CORE_LOCATION_H_
