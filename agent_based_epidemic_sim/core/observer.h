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

#ifndef AGENT_BASED_EPIDEMIC_SIM_CORE_OBSERVER_H_
#define AGENT_BASED_EPIDEMIC_SIM_CORE_OBSERVER_H_

#include <memory>

#include "absl/container/flat_hash_set.h"
#include "absl/types/span.h"
#include "agent_based_epidemic_sim/core/event.h"
#include "agent_based_epidemic_sim/core/timestep.h"
#include "agent_based_epidemic_sim/core/visit.h"

// This file defines interfaces for observing the simulation for output and
// recording of statistics.  Recording output from a simulation is a three step
// process. We'll explain with an example. Suppose we want to record a histogram
// of the number of visits locations receive in a single timestep:
//
// Step 1: Implement the Observer.
// Write a class that implements one or more of the obsever interfaces.  This is
// the code that will actually look at data as the simulation progresses.
// Simulators may create multiple instances of these observers for each timestep
// of the simulation, but a single observer will only live for one timestep.
//
// class LocationVisitHistogramObserver : public LocationVisitObserver {
//  public:
//   void Observe(uint64 location_uuid, absl::Span<const Visit> visits) override
//   {
//     hist_.resize(std::max(hist_.size(), visits.size() + 1));
//     hist_[visits.size()]++;
//   }
//
//  private:
//   friend void AggregateLocationVisitHistograms(
//       const Timestep& timestep,
//       absl::Span<LocationVisitHistogramObserver* const> observers);
//   std::vector<int> hist_;
// };
//
// Step 2: Implement the ObserverFactory.
// As stated above, the simulator may need to instantiate multiple observers per
// timestep of simulation, we do this so we can record statistics efficiently
// in a multi-threaded environment.  The factory is responsable not only for
// creating the observers, but also aggregating them at the end of a timestep.
//
// class LocationVisitHistogramFactory : public ObserverFactory {
//  public:
//   using ObserverPtr = std::unique_ptr<LocationVisitHistogramObserver>;
//   std::unique_ptr<LocationVisitHistoryObserver> MakeObserver() override {
//     return absl::make_unique<LocationVisitHistoryObserver>();
//   }
//   void Aggregate(
//     const Timestep& timestep,
//     absl::Span<const ObserverPtr> observers) override {
//       std::vector<int> hist;
//       for (auto& observer : observers) {
//       hist.resize(std::max(hist.size(),observer->hist_.size()));
//       for (int i = 0; i < hist.size(); ++i)
//       {
//         hist[i] += observer->hist_[i];
//       }
//     }
//     // Perhaps write the histogram for this timestep to a csv file...
//   }
// };
//
// Step 3: Register an instance of the ObserverFactory.
// We need to register the factory with the simulator, after we do that all
// succeeding steps of the simulation will use our factory to create and
// aggregate observer.
//
// LocationVisitHistogramFactory factory;
// Simulaton* sim = ...;
// sim->AddObserverFactory(&factory);
//
// The interfaces here are designed so users need not understand the
// threading model of the simulation they are observing. The methods of objects
// deriving from one or more Observer interfaces will not be called concurrently
// by multiple threads. Similarly ObserverFactory methods will not be called
// concurrently by multiple threads.

namespace abesim {

class Agent;
class Location;
class ObserverShard;

class AgentInfectionObserver {
 public:
  virtual ~AgentInfectionObserver() = default;
  // Observes all the InfectionOutcomes for a given agent for this timestep.
  virtual void Observe(const Agent&, absl::Span<const InfectionOutcome>) = 0;
};

class LocationVisitObserver {
 public:
  virtual ~LocationVisitObserver() = default;
  // Observes all the visits to a given location for this timestep.
  virtual void Observe(const Location&, absl::Span<const Visit>) = 0;
};

// ObserverFactoryBase is the base for all ObserverFactory instances, but
// Observer implementors should implement the more convenient ObserverFactory
// interface instead.
class ObserverFactoryBase {
 public:
  virtual ~ObserverFactoryBase() = default;

 private:
  friend class ObserverManager;
  virtual void MakeObserverForShard(const Timestep&, ObserverShard*) = 0;
  virtual void Aggregate(const Timestep&) = 0;
};

template <typename Observer>
class ObserverFactory : public ObserverFactoryBase {
 public:
  // Makes a new Observer instance.  Observers only live for a single timestep.
  // Many observer instances may be produced for a given timestep, and all
  // the observers that are produced will be passed to Aggregate at the end of
  // the timestep.
  virtual std::unique_ptr<Observer> MakeObserver(
      const Timestep& timestep) const = 0;

  // Aggregates the data from many Observers.  This is called at the end of each
  // timestep.
  virtual void Aggregate(
      const Timestep& timestep,
      absl::Span<std::unique_ptr<Observer> const> observers) = 0;

 private:
  void MakeObserverForShard(const Timestep& timestep,
                            ObserverShard* shard) override;
  void Aggregate(const Timestep& timestep) override;

  std::vector<std::unique_ptr<Observer>> observers_;
};

// An ObserverShard is a view onto the set of observers being used in a
// single worker thread.  This is used by Simulator implementations to manage
// observers, and is only interesting to Simulator authors.
class ObserverShard : public AgentInfectionObserver,
                      public LocationVisitObserver {
 public:
  void Observe(const Agent& agent,
               absl::Span<const InfectionOutcome> outcomes) override;
  void Observe(const Location& location,
               absl::Span<const Visit> visits) override;

 private:
  template <typename Observer>
  friend class ObserverFactory;

  template <typename Observer>
  void RegisterObserver(Observer* observer);

  std::vector<AgentInfectionObserver*> agent_infection_observers_;
  std::vector<LocationVisitObserver*> location_visit_observers_;
};

// ObserverManager manages a set of observer factories.  Creating ObserverShards
// for worker threads, calling Aggregate on the factories at the end of each
// timestep.  Observer manager is not threadsafe, its methods should not be
// called concurrently.  Different ObserverShards may be used by different
// threads, but no single ObserverShard should be used concurrently by multiple
// threads. This is used by Simulator implementations to manage observers, and
// is only interesting to Simulator authors.
class ObserverManager {
 public:
  // Adds an ObserverFactory.  factory->Aggregate will be called
  // for all following simulation steps until the factory is removed.
  void AddFactory(ObserverFactoryBase* factory);
  // Removes an ObserverFactory.
  void RemoveFactory(ObserverFactoryBase* factory);
  // Calls ObserverFactory::Aggregate for all added factories.
  void AggregateForTimestep(const Timestep& timestep);
  // Make a new ObserverShard that can be used by a worker thread to report
  // observations.  Note that the manager retains ownership and that the
  // returned pointer will only be valid until the next call to
  // AggregateTimestep.
  ObserverShard* MakeShard(const Timestep& timestep);

 private:
  friend class ObserverShard;
  void RegisterObservers(const Timestep& timestep, ObserverShard* shard);

  absl::flat_hash_set<ObserverFactoryBase*> factories_;
  std::vector<std::unique_ptr<ObserverShard>> shards_;
};

template <typename Observer>
void ObserverFactory<Observer>::Aggregate(const Timestep& timestep) {
  Aggregate(timestep, observers_);
  observers_.clear();
}

template <typename Observer>
void ObserverFactory<Observer>::MakeObserverForShard(const Timestep& timestep,
                                                     ObserverShard* shard) {
  observers_.push_back(MakeObserver(timestep));
  shard->RegisterObserver(observers_.back().get());
}

template <typename Observer>
void ObserverShard::RegisterObserver(Observer* observer) {
  if constexpr (std::is_base_of_v<AgentInfectionObserver, Observer>) {
    agent_infection_observers_.push_back(observer);
  }
  if constexpr (std::is_base_of_v<LocationVisitObserver, Observer>) {
    location_visit_observers_.push_back(observer);
  }
}

}  // namespace abesim

#endif  // AGENT_BASED_EPIDEMIC_SIM_CORE_OBSERVER_H_
