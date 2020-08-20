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

#ifndef AGENT_BASED_EPIDEMIC_SIM_CORE_EXPOSURE_GENERATOR_BUILDER_H_
#define AGENT_BASED_EPIDEMIC_SIM_CORE_EXPOSURE_GENERATOR_BUILDER_H_

#include <memory>
#include <vector>

#include "agent_based_epidemic_sim/core/exposure_generator.h"

namespace abesim {

class ExposureGeneratorBuilder {
 public:
  ExposureGeneratorBuilder() = default;
  virtual ~ExposureGeneratorBuilder() = default;

  // TODO: Change the signature of this method to
  // std::vector<ProximityTrace>& proximity_trace_distribution once the CSV
  // format containing the distribution is standardized and parsing logic has
  // been implemented. Currently the distribution is defined in constants.h and
  // uses std::vector for each proximity trace rather than std::array to avoid
  // copying std::numeric_limits<float>::max() a number of times. Also consider
  // returning absl::StatusOr and return non-ok status if
  // proximity_trace_distribution is empty. In principle we should let the
  // highest level handle this error instead of CHECK failing in the
  // implementation.
  virtual std::unique_ptr<ExposureGenerator> Build(
      const std::vector<std::vector<float>>& proximity_trace_distribution)
      const = 0;
};

}  // namespace abesim

#endif  // AGENT_BASED_EPIDEMIC_SIM_CORE_EXPOSURE_GENERATOR_BUILDER_H_
