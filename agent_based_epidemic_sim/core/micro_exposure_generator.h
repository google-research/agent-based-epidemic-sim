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

#ifndef AGENT_BASED_EPIDEMIC_SIM_CORE_MICRO_EXPOSURE_GENERATOR_H_
#define AGENT_BASED_EPIDEMIC_SIM_CORE_MICRO_EXPOSURE_GENERATOR_H_

#include <array>

#include "absl/random/random.h"
#include "agent_based_epidemic_sim/core/event.h"
#include "agent_based_epidemic_sim/core/exposure_generator.h"
#include "agent_based_epidemic_sim/port/logging.h"

namespace abesim {

class MicroExposureGenerator : public ExposureGenerator {
 public:
  explicit MicroExposureGenerator(
      const std::vector<ProximityTrace>& proximity_trace_distribution)
      : proximity_trace_distribution_(proximity_trace_distribution) {}

  virtual ~MicroExposureGenerator() = default;
  // Generate a pair of Exposure objects representing a single contact between
  // two hosts. The first and second values in the infectivity and
  // symptom_factor pairs will correspond to the first and second Exposures
  // returned, respectively.
  ExposurePair Generate(const HostData& host_a,
                        const HostData& host_b) override;

 private:
  // Draws a proximity trace from an in-memory, non-parametric distribution.
  // Represents the distances between two hosts at fixed intervals.
  ProximityTrace DrawProximityTrace();

  // Generates a proximity trace by drawing from a uniform distribution.
  // Represents the distances between two hosts at fixed intervals.
  ProximityTrace GenerateProximityTrace();

  std::vector<ProximityTrace> proximity_trace_distribution_;
  absl::BitGen gen_;
};

}  // namespace abesim

#endif  // AGENT_BASED_EPIDEMIC_SIM_CORE_MICRO_EXPOSURE_GENERATOR_H_
