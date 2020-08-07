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

#ifndef AGENT_BASED_EPIDEMIC_SIM_APPLICATIONS_HOME_WORK_SIMULATION_H_
#define AGENT_BASED_EPIDEMIC_SIM_APPLICATIONS_HOME_WORK_SIMULATION_H_

#include "absl/strings/string_view.h"
#include "agent_based_epidemic_sim/agent_synthesis/population_profile.pb.h"
#include "agent_based_epidemic_sim/applications/home_work/config.pb.h"
#include "agent_based_epidemic_sim/applications/home_work/location_type.h"
#include "agent_based_epidemic_sim/core/risk_score.h"

namespace abesim {

// TODO: Encapsulate policy generator and location type context here.
struct SimulationContext {
  std::vector<AgentProto> agents;
  std::vector<LocationProto> locations;
  LocationTypeFn location_type;
  PopulationProfiles population_profiles;
};

SimulationContext GetSimulationContext(const HomeWorkSimulationConfig& config);

// Runs a home-work-home simulation from config.
void RunSimulation(absl::string_view output_file_path,
                   absl::string_view learning_output_base,
                   const HomeWorkSimulationConfig& config, int num_workers);

// Runs a simulation for a collection of agents and locations.
// Question: Make this more generic? Just pass non-home/work-specific objects
// v<LocationProto>, v<AgentProto>, PopulationProfiles, generic config such as
// timestep info, std::unique_ptr<PolicyGenerator>, and output_file_path? In
// this scenario, location_type_fn could be managed within SimulationObjects.
void RunSimulation(
    absl::string_view output_file_path, absl::string_view learning_output_base,
    const HomeWorkSimulationConfig& config,
    const std::function<std::unique_ptr<RiskScoreGenerator>(LocationTypeFn)>&
        get_risk_score_generator,
    int num_workers, const SimulationContext& context);

}  // namespace abesim

#endif  // AGENT_BASED_EPIDEMIC_SIM_APPLICATIONS_HOME_WORK_SIMULATION_H_
