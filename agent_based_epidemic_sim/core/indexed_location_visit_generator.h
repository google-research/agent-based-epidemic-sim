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

#ifndef AGENT_BASED_EPIDEMIC_SIM_CORE_INDEXED_LOCATION_VISIT_GENERATOR_H_
#define AGENT_BASED_EPIDEMIC_SIM_CORE_INDEXED_LOCATION_VISIT_GENERATOR_H_

#include "absl/random/random.h"
#include "absl/time/time.h"
#include "agent_based_epidemic_sim/core/event.h"
#include "agent_based_epidemic_sim/core/risk_score.h"
#include "agent_based_epidemic_sim/core/timestep.h"
#include "agent_based_epidemic_sim/core/visit.h"
#include "agent_based_epidemic_sim/core/visit_generator.h"

namespace abesim {

// Generates visits to a set of locations with a random duration.
// All locations are covered in each call to GenerateVisit, with the total
// duration summing to timestep.
// Advances the internal clock by timestep.
class IndexedLocationVisitGenerator : public VisitGenerator {
 public:
  explicit IndexedLocationVisitGenerator(
      const std::vector<int64>& location_uuids);

  void GenerateVisits(const Timestep& timestep, const RiskScore& risk_score,
                      std::vector<Visit>* visits) override;

 private:
  absl::BitGen gen_;
  std::unique_ptr<VisitGenerator> visit_generator_;
};

}  // namespace abesim

#endif  // AGENT_BASED_EPIDEMIC_SIM_CORE_INDEXED_LOCATION_VISIT_GENERATOR_H_
