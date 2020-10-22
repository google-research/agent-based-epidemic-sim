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
#include "agent_based_epidemic_sim/core/random.h"

namespace abesim {

ExposurePair MicroExposureGenerator::Generate(float location_transmissibility,
                                              const Visit& visit_a,
                                              const Visit& visit_b) const {
  const ProximityTrace proximity_trace = proximity_trace_distribution_.empty()
                                             ? GenerateProximityTrace()
                                             : DrawProximityTrace();

  int trace_length =
      std::count_if(proximity_trace.values.begin(),
                    proximity_trace.values.end(), [](float proximity) {
                      return proximity < std::numeric_limits<float>::max();
                    });
  const absl::Duration trace_duration = trace_length * kProximityTraceInterval;
  absl::Time start_time = std::max(visit_a.start_time, visit_b.start_time);

  // TODO: Figure out how to remove proximity_trace from Exposure
  // message while still threading it through here somehow.
  return {.host_a =
              {
                  .start_time = start_time,
                  .duration = trace_duration,
                  .proximity_trace = proximity_trace,
                  // TODO: Is it right that infectivity and
                  // symptom_factor are set to the same value?
                  .infectivity = visit_b.infectivity,
                  .symptom_factor = visit_b.infectivity,
                  .location_transmissibility = location_transmissibility,
              },
          .host_b = {
              .start_time = start_time,
              .duration = trace_duration,
              .proximity_trace = proximity_trace,
              .infectivity = visit_a.infectivity,
              .symptom_factor = visit_a.symptom_factor,
              .location_transmissibility = location_transmissibility,
          }};
}

ProximityTrace MicroExposureGenerator::GenerateProximityTrace() const {
  ProximityTrace full_length_proximity_trace;
  full_length_proximity_trace.values.fill(std::numeric_limits<float>::max());

  absl::BitGenRef gen = GetBitGen();
  int proximity_trace_length = absl::Uniform<int>(gen, 1, kMaxTraceLength);
  for (int i = 0; i < proximity_trace_length; ++i) {
    full_length_proximity_trace.values[i] =
        absl::Uniform<float>(gen, 0.0f, 10.0f);
  }
  return full_length_proximity_trace;
}

ProximityTrace MicroExposureGenerator::DrawProximityTrace() const {
  return proximity_trace_distribution_[absl::Uniform<int>(
      GetBitGen(), 0, proximity_trace_distribution_.size() - 1)];
}

}  // namespace abesim
