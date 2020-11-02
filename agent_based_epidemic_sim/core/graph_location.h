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

#include "absl/types/span.h"
#include "agent_based_epidemic_sim/core/exposure_generator.h"
#include "agent_based_epidemic_sim/core/integral_types.h"
#include "agent_based_epidemic_sim/core/location.h"

namespace abesim {

// Creates a new location that samples edges from the given graph of possible
// agent  connections.  drop_probability indicates the probability that a given
// connection should be ignored on each ProcessVisits call.
// location_transmissibility is a function that returns the transmissibility
// factor of this location and should be a floating ponit number between 0
// and 1.  It is taken as a function because the value may change from one
// timestep to another.
std::unique_ptr<Location> NewGraphLocation(
    int64 uuid, std::function<float()> location_transmissibility,
    std::function<float()> drop_probability,
    std::vector<std::pair<int64, int64>> graph,
    const ExposureGenerator& exposure_generator);

// Creates a new location that dynamically connects visiting agents. On each
// call to ProcessVisits, samples edges between all agents with visits to the
// location. The number of edges for each agent is taken from the
// VisitLocationDynamics of each agents visit message.
std::unique_ptr<Location> NewRandomGraphLocation(
    int64 uuid, std::function<float()> location_transmissibility,
    std::function<float()> lockdown_multiplier,
    const ExposureGenerator& exposure_generator);

// Internal namespace exposed for testing.

namespace internal {

// Extracts a list of agent UUIDs from visits. An agent is repeated once for
// each edge needed by it.
void AgentUuidsFromRandomLocationVisits(absl::Span<const Visit> visits,
                                        float lockdown_multiplier,
                                        std::vector<int64>& agent_uuids);

// Constructs a graph by adding edges between adjacent elements of agent_uuids,
// except where adjacent elements are identical.
// E.g. given [a, b, a, c, c, c, d] constructs graph with edges [a-b, a-c, c-d].
void ConnectAdjacentNodes(absl::Span<const int64> agent_uuids,
                          std::vector<std::pair<int64, int64>>& graph);

}  // namespace internal
}  // namespace abesim

#endif  // AGENT_BASED_EPIDEMIC_SIM_CORE_GRAPH_LOCATION_H_
