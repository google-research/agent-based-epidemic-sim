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

#include "agent_based_epidemic_sim/core/micro_exposure_generator.h"

#include <algorithm>
#include <array>
#include <limits>
#include <vector>

#include "absl/random/distributions.h"
#include "absl/time/time.h"
#include "agent_based_epidemic_sim/core/constants.h"
#include "agent_based_epidemic_sim/core/event.h"
#include "agent_based_epidemic_sim/core/exposure_generator.h"
#include "agent_based_epidemic_sim/core/parameter_distribution.pb.h"

namespace abesim {

ExposurePair MicroExposureGenerator::Generate(const HostData& host_a,
                                              const HostData& host_b) {
  const ProximityTrace proximity_trace = proximity_trace_distribution_.empty()
                                             ? GenerateProximityTrace()
                                             : DrawProximityTrace();

  int trace_length =
      std::count_if(proximity_trace.values.begin(),
                    proximity_trace.values.end(), [](float proximity) {
                      return proximity < std::numeric_limits<float>::max();
                    });
  const absl::Duration trace_duration = trace_length * kProximityTraceInterval;

  return {.host_a =
              {
                  .duration = trace_duration,
                  .proximity_trace = proximity_trace,
                  .infectivity = host_b.infectivity,
                  .symptom_factor = host_b.infectivity,
              },
          .host_b = {
              .duration = trace_duration,
              .proximity_trace = proximity_trace,
              .infectivity = host_a.infectivity,
              .symptom_factor = host_a.symptom_factor,
          }};
}

ProximityTrace MicroExposureGenerator::GenerateProximityTrace() {
  ProximityTrace full_length_proximity_trace;
  full_length_proximity_trace.values.fill(std::numeric_limits<float>::max());

  int proximity_trace_length = absl::Uniform<int>(gen_, 1, kMaxTraceLength);
  for (int i = 0; i < proximity_trace_length; ++i) {
    full_length_proximity_trace.values[i] =
        absl::Uniform<float>(gen_, 0.0f, 10.0f);
  }
  return full_length_proximity_trace;
}

ProximityTrace MicroExposureGenerator::DrawProximityTrace() {
  return proximity_trace_distribution_[absl::Uniform<int>(
      gen_, 0, proximity_trace_distribution_.size() - 1)];
}

}  // namespace abesim
