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

#ifndef AGENT_BASED_EPIDEMIC_SIM_CORE_VISIT_H_
#define AGENT_BASED_EPIDEMIC_SIM_CORE_VISIT_H_

#include "absl/meta/type_traits.h"
#include "absl/time/time.h"
#include "agent_based_epidemic_sim/core/integral_types.h"
#include "agent_based_epidemic_sim/core/pandemic.pb.h"

namespace abesim {

// A duration of time for which an agent is in a health state.
struct HealthInterval {
  absl::Time start_time;
  absl::Time end_time;
  HealthState::State health_state;

  friend bool operator==(const HealthInterval& a, const HealthInterval& b) {
    return (a.start_time == b.start_time && a.end_time == b.end_time &&
            a.health_state == b.health_state);
  }

  friend bool operator!=(const HealthInterval& a, const HealthInterval& b) {
    return !(a == b);
  }

  friend std::ostream& operator<<(std::ostream& strm,
                                  const HealthInterval& health_interval) {
    return strm << "{" << health_interval.start_time << ", "
                << health_interval.end_time << ", "
                << health_interval.health_state << "}";
  }
};

static_assert(absl::is_trivially_copy_constructible<HealthInterval>::value,
              "Event must be trivially copyable.");

// A visit to a given location in time of an agent.
struct Visit {
  int64 location_uuid;
  int64 agent_uuid;
  absl::Time start_time;
  absl::Time end_time;
  HealthState::State health_state;

  friend bool operator==(const Visit& a, const Visit& b) {
    return (a.location_uuid == b.location_uuid &&
            a.agent_uuid == b.agent_uuid && a.start_time == b.start_time &&
            a.end_time == b.end_time && a.health_state == b.health_state);
  }

  friend bool operator!=(const Visit& a, const Visit& b) { return !(a == b); }

  friend std::ostream& operator<<(std::ostream& strm, const Visit& visit) {
    return strm << "{" << visit.location_uuid << ", " << visit.agent_uuid
                << ", " << visit.start_time << ", " << visit.end_time << ", "
                << visit.health_state << "}";
  }
};

static_assert(absl::is_trivially_copy_constructible<Visit>::value,
              "Event must be trivially copyable.");

}  // namespace abesim

#endif  // AGENT_BASED_EPIDEMIC_SIM_CORE_VISIT_H_
