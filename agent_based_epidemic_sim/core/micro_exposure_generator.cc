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

#include <iostream>

#include "absl/time/time.h"
#include "agent_based_epidemic_sim/core/event.h"
#include "agent_based_epidemic_sim/core/parameter_distribution.pb.h"

namespace abesim {

Exposure MicroExposureGenerator::Generate(absl::Time start_time,  // unused
                                          absl::Duration duration,
                                          float infectivity,
                                          float symptom_factor) {
  return {
      .duration = duration,
      .infectivity = infectivity,
      .symptom_factor = symptom_factor,
  };
}

}  // namespace abesim
