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

#ifndef AGENT_BASED_EPIDEMIC_SIM_CORE_EXPOSURE_GENERATOR_H_
#define AGENT_BASED_EPIDEMIC_SIM_CORE_EXPOSURE_GENERATOR_H_

#include "absl/time/time.h"
#include "agent_based_epidemic_sim/core/event.h"

namespace abesim {

struct HostData {
  absl::Time start_time;
  float infectivity;
  float symptom_factor;
};

struct ExposurePair {
  Exposure host_a;
  Exposure host_b;
};

// Implement this interface to generate exposures based on a custom scheme.
class ExposureGenerator {
 public:
  virtual ~ExposureGenerator() = default;
  // TODO: Incorporate a notion of gauranteed exposure duration
  // into this method.
  // Returns a pair of Exposures mirroring a single exposure event between a
  // pair of hosts.
  virtual ExposurePair Generate(const HostData& host_a,
                                const HostData& host_b) = 0;
};

}  // namespace abesim

#endif  // AGENT_BASED_EPIDEMIC_SIM_CORE_EXPOSURE_GENERATOR_H_
