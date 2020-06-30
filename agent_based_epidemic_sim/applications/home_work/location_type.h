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

#ifndef THIRD_PARTY_AGENT_BASED_EPIDEMIC_SIM_APPLICATIONS_HOME_WORK_LOCATION_TYPE_H_
#define THIRD_PARTY_AGENT_BASED_EPIDEMIC_SIM_APPLICATIONS_HOME_WORK_LOCATION_TYPE_H_

#include <functional>
#include <initializer_list>

#include "agent_based_epidemic_sim/core/integral_types.h"

namespace abesim {

enum class LocationType : uint8 { kHome, kWork };

constexpr std::initializer_list<LocationType> kAllLocationTypes = {
    LocationType::kHome, LocationType::kWork};

using LocationTypeFn = std::function<LocationType(int64 uuid)>;

}  // namespace abesim

#endif  // THIRD_PARTY_AGENT_BASED_EPIDEMIC_SIM_APPLICATIONS_HOME_WORK_LOCATION_TYPE_H_
