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

#include "agent_based_epidemic_sim/core/indexed_location_visit_generator.h"

#include "absl/random/uniform_real_distribution.h"
#include "agent_based_epidemic_sim/core/duration_specified_visit_generator.h"

namespace abesim {
namespace {
constexpr float kEpsilon = 1e-5;
}  // namespace

IndexedLocationVisitGenerator::IndexedLocationVisitGenerator(
    const std::vector<int64>& location_uuids) {
  std::vector<LocationDuration> location_durations;
  location_durations.reserve(location_uuids.size());
  for (const int64 location_uuid : location_uuids) {
    location_durations.push_back(
        {.location_uuid = location_uuid,
         .sample_duration = [this](float adjustment) {
           return absl::uniform_real_distribution<float>(
               kEpsilon, adjustment - kEpsilon)(gen_);
         }});
  }
  visit_generator_ =
      absl::make_unique<DurationSpecifiedVisitGenerator>(location_durations);
}

void IndexedLocationVisitGenerator::GenerateVisits(const Timestep& timestep,
                                                   const RiskScore& risk_score,
                                                   std::vector<Visit>* visits) {
  visit_generator_->GenerateVisits(timestep, risk_score, visits);
}

}  // namespace abesim
