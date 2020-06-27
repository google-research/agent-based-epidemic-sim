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

#ifndef THIRD_PARTY_AGENT_BASED_EPIDEMIC_SIM_CORE_LOCATION_DISCRETE_EVENT_SIMULATOR_H_
#define THIRD_PARTY_AGENT_BASED_EPIDEMIC_SIM_CORE_LOCATION_DISCRETE_EVENT_SIMULATOR_H_

#include "agent_based_epidemic_sim/core/broker.h"
#include "agent_based_epidemic_sim/core/integral_types.h"
#include "agent_based_epidemic_sim/core/location.h"
#include "agent_based_epidemic_sim/core/observer.h"
#include "agent_based_epidemic_sim/core/transmission_model.h"
#include "agent_based_epidemic_sim/core/visit.h"

namespace pandemic {

// Implements a sequential discrete event simulator for a Location.
class LocationDiscreteEventSimulator : public Location {
 public:
  explicit LocationDiscreteEventSimulator(const int64 uuid) : uuid_(uuid) {}

  int64 uuid() const override { return uuid_; }

  void ProcessVisits(absl::Span<const Visit> visits,
                     Broker<InfectionOutcome>* infection_broker) override;

 private:
  const int64 uuid_;
};

}  // namespace pandemic

#endif  // THIRD_PARTY_AGENT_BASED_EPIDEMIC_SIM_CORE_LOCATION_DISCRETE_EVENT_SIMULATOR_H_
