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

#ifndef AGENT_BASED_EPIDEMIC_SIM_CORE_SIMULATION_H_
#define AGENT_BASED_EPIDEMIC_SIM_CORE_SIMULATION_H_

#include "agent_based_epidemic_sim/core/agent.h"
#include "agent_based_epidemic_sim/core/distributed.h"
#include "agent_based_epidemic_sim/core/location.h"
#include "agent_based_epidemic_sim/core/observer.h"

namespace abesim {

// Simulation is the primary interface for managing pandemic simulations.
// Simulations are not threadsafe, their methods should not be called
// concurrently.
class Simulation {
 public:
  // Simulate `steps` steps, each of duration `timestep`.
  // A single step consists of:
  // 1) Agents process outcomes from the previous step and generate visits.
  // 2) Locations process these place visits and generate outcomes.
  virtual void Step(int steps, absl::Duration step_duration) = 0;

  // Add an ObserverFactory.  ObserverFactories are used to create Observers
  // which can inspect the agents and locations and various other objects as the
  // simulation progresses.  Registering an ObserverFactory is the primary way
  // to generate statistics and output from simulations.
  virtual void AddObserverFactory(ObserverFactoryBase* factory) = 0;

  // Remove an ObserverFactory.  Future timesteps will not use the given
  // factory.
  virtual void RemoveObserverFactory(ObserverFactoryBase* factory) = 0;

  virtual ~Simulation() = default;
};

std::unique_ptr<Simulation> SerialSimulation(
    absl::Time start, std::vector<std::unique_ptr<Agent>> agents,
    std::vector<std::unique_ptr<Location>> locations);

std::unique_ptr<Simulation> ParallelSimulation(
    absl::Time start, std::vector<std::unique_ptr<Agent>> agents,
    std::vector<std::unique_ptr<Location>> locations, int num_workers);

// Create a parallel simulation with num_local_workers local worker threads
// and also coorinate with distributed simulation nodes via the given
// DistributedManager.
std::unique_ptr<Simulation> ParallelDistributedSimulation(
    absl::Time start, std::vector<std::unique_ptr<Agent>> agents,
    std::vector<std::unique_ptr<Location>> locations, int num_local_workers,
    DistributedManager* distributed_manager);

}  // namespace abesim

#endif  // AGENT_BASED_EPIDEMIC_SIM_CORE_SIMULATION_H_
