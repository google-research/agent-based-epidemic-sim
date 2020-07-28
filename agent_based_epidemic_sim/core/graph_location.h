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

#ifndef AGENT_BASED_EPIDEMIC_SIM_CORE_GRAPH_LOCATION_H_
#define AGENT_BASED_EPIDEMIC_SIM_CORE_GRAPH_LOCATION_H_

#include <memory>

#include "agent_based_epidemic_sim/core/integral_types.h"
#include "agent_based_epidemic_sim/core/location.h"

namespace abesim {

// Create a new location that samples edges from the given graph of possible
// agent  connections.  drop_probability indicates the probability that a given
// connection should be ignored on each ProcessVisits call.
std::unique_ptr<Location> NewGraphLocation(
    int64 uuid, float drop_probability,
    std::vector<std::pair<int64, int64>> graph);

}  // namespace abesim

#endif  // AGENT_BASED_EPIDEMIC_SIM_CORE_GRAPH_LOCATION_H_
