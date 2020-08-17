// Copyright 2020 Google LLC
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//      http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#include "agent_based_epidemic_sim/core/observer.h"

namespace abesim {

void ObserverManager::AddFactory(ObserverFactoryBase* factory) {
  factories_.insert(factory);
}

void ObserverManager::RemoveFactory(ObserverFactoryBase* factory) {
  factories_.erase(factory);
}

void ObserverManager::AggregateForTimestep(const Timestep& timestep) {
  for (auto& factory : factories_) {
    factory->Aggregate(timestep);
  }
  shards_.clear();
}

ObserverShard* ObserverManager::MakeShard(const Timestep& timestep) {
  shards_.push_back(absl::make_unique<ObserverShard>());
  for (ObserverFactoryBase* factory : factories_) {
    factory->MakeObserverForShard(timestep, shards_.back().get());
  }
  return shards_.back().get();
}

void ObserverManager::RegisterObservers(const Timestep& timestep,
                                        ObserverShard* shard) {
  for (auto* factory : factories_) {
    factory->MakeObserverForShard(timestep, shard);
  }
}

void ObserverShard::Observe(const Agent& agent,
                            absl::Span<const InfectionOutcome> outcomes) {
  for (AgentInfectionObserver* observer : agent_infection_observers_) {
    observer->Observe(agent, outcomes);
  }
}
void ObserverShard::Observe(const Location& location,
                            absl::Span<const Visit> visits) {
  for (LocationVisitObserver* observer : location_visit_observers_) {
    observer->Observe(location, visits);
  }
}

}  // namespace abesim
