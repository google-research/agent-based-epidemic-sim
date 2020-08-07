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

#include "agent_based_epidemic_sim/core/duration_specified_visit_generator.h"

#include "absl/random/distributions.h"
#include "absl/time/time.h"
#include "agent_based_epidemic_sim/port/logging.h"

namespace abesim {

void DurationSpecifiedVisitGenerator::GenerateVisits(
    const Timestep& timestep, const RiskScore& risk_score,
    std::vector<Visit>* visits) {
  DCHECK(visits != nullptr);
  std::vector<float> durations;
  for (const LocationDuration& location_duration : location_durations_) {
    auto adjustment = risk_score.GetVisitAdjustment(
        timestep, location_duration.location_uuid);
    if (!absl::Bernoulli(gen_, adjustment.frequency_adjustment)) {
      durations.push_back(0.0);
    } else {
      float sample =
          location_duration.sample_duration(adjustment.duration_adjustment);
      durations.push_back(std::max(0.0f, sample));
    }
  }
  float normalizer = std::accumulate(durations.begin(), durations.end(), 0.0f);
  if (normalizer == 0.0f) {
    // Agents have to be somewhere.  If they don't sample any location, then
    // just send them to their first location all day.
    normalizer = durations[0] = 1.0f;
  }
  absl::Time start_time = timestep.start_time();
  for (int i = 0; i < location_durations_.size(); ++i) {
    absl::Time end_time;
    if (i == location_durations_.size() - 1) {
      end_time = timestep.end_time();
    } else {
      end_time = std::min(
          timestep.end_time(),
          start_time + (durations[i] / normalizer) * timestep.duration());
    }
    if (end_time <= start_time) continue;
    Visit visit{.location_uuid = location_durations_[i].location_uuid,
                .start_time = start_time,
                .end_time = end_time};
    start_time = end_time;
    visits->push_back(visit);
  }
}

}  // namespace abesim
