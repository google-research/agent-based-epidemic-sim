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

#ifndef THIRD_PARTY_AGENT_BASED_EPIDEMIC_SIM_CORE_LOCATION_DISCRETE_EVENT_SIMULATOR_BUILDER_H_
#define THIRD_PARTY_AGENT_BASED_EPIDEMIC_SIM_CORE_LOCATION_DISCRETE_EVENT_SIMULATOR_BUILDER_H_

#include "agent_based_epidemic_sim/core/location.h"
#include "agent_based_epidemic_sim/core/uuid_generator.h"

namespace pandemic {

// Builds a location.
class LocationDiscreteEventSimulatorBuilder {
 public:
  LocationDiscreteEventSimulatorBuilder(
      std::unique_ptr<UuidGenerator> uuid_generator)
      : uuid_generator_(std::move(uuid_generator)) {}

  std::unique_ptr<Location> Build() const;

 private:
  std::unique_ptr<UuidGenerator> uuid_generator_;
};

}  // namespace pandemic

#endif  // THIRD_PARTY_AGENT_BASED_EPIDEMIC_SIM_CORE_LOCATION_DISCRETE_EVENT_SIMULATOR_BUILDER_H_
