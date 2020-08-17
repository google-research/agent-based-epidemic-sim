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

#ifndef AGENT_BASED_EPIDEMIC_SIM_APPLICATIONS_HOME_WORK_OBSERVER_H_
#define AGENT_BASED_EPIDEMIC_SIM_APPLICATIONS_HOME_WORK_OBSERVER_H_

#include "absl/container/flat_hash_map.h"
#include "absl/container/flat_hash_set.h"
#include "absl/time/time.h"
#include "agent_based_epidemic_sim/applications/home_work/location_type.h"
#include "agent_based_epidemic_sim/core/enum_indexed_array.h"
#include "agent_based_epidemic_sim/core/integral_types.h"
#include "agent_based_epidemic_sim/core/observer.h"
#include "agent_based_epidemic_sim/port/file_utils.h"

namespace abesim {

template <typename T>
using HealthArray =
    EnumIndexedArray<T, HealthState::State, HealthState::State_ARRAYSIZE>;
template <typename T>
using LocationArray =
    EnumIndexedArray<T, LocationType, kAllLocationTypes.size()>;

class HomeWorkSimulationObserver : public AgentInfectionObserver,
                                   public LocationVisitObserver {
 public:
  explicit HomeWorkSimulationObserver(LocationTypeFn location_type);

  void Observe(const Agent& agent,
               absl::Span<const InfectionOutcome> outcomes) override;
  void Observe(const Location& location,
               absl::Span<const Visit> visits) override;

 private:
  friend class HomeWorkSimulationObserverFactory;

  const LocationTypeFn location_type_;
  HealthArray<int> health_state_counts_;
  absl::flat_hash_map<int64, LocationArray<absl::Duration>>
      agent_location_type_durations_;
  absl::flat_hash_map<int64, absl::flat_hash_set<int64>> contacts_;
};

// This aggregator assumes single node execution.
class HomeWorkSimulationObserverFactory
    : public ObserverFactory<HomeWorkSimulationObserver> {
 public:
  // Creates a new HomeWorkSimulationObserverFactory that will write to file.
  // The given location_type function will be used to distinguish different
  // types of locations based on the uuid of the location.
  // Use pass_through_fields to append a set of field values to every line
  // of the csv output, each entry is a pair of {field_name, field_value}.
  explicit HomeWorkSimulationObserverFactory(
      file::FileWriter* output,
      std::function<LocationType(int64)> location_type,
      const std::vector<std::pair<std::string, std::string>>&
          pass_through_fields);

  void Aggregate(const Timestep& timestep,
                 absl::Span<std::unique_ptr<HomeWorkSimulationObserver> const>
                     observers) override;
  std::unique_ptr<HomeWorkSimulationObserver> MakeObserver(
      const Timestep& timestep) const override;

  absl::Status status() const { return status_; }

 private:
  file::FileWriter* const output_;
  const LocationTypeFn location_type_;
  std::string data_prefix_;

  absl::Status status_;
  HealthArray<int> health_state_counts_;
  absl::flat_hash_map<int64, LocationArray<absl::Duration>>
      agent_location_type_durations_;
  absl::flat_hash_map<int64, absl::flat_hash_set<int64>> contacts_;
};

}  // namespace abesim

#endif  // AGENT_BASED_EPIDEMIC_SIM_APPLICATIONS_HOME_WORK_OBSERVER_H_
