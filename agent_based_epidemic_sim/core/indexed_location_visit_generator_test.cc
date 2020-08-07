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

#include "absl/time/time.h"
#include "agent_based_epidemic_sim/core/risk_score.h"
#include "agent_based_epidemic_sim/core/visit.h"
#include "gtest/gtest.h"

namespace abesim {
namespace {

absl::Duration VisitDuration(const Visit& visit) {
  return visit.end_time - visit.start_time;
}

TEST(IndexedLocationVisitGeneratorTest, GeneratesVisits) {
  std::vector<int64> location_uuids{0LL, 1LL, 2LL};
  IndexedLocationVisitGenerator visit_generator(location_uuids);
  auto risk_score = NewNullRiskScore();

  std::vector<Visit> visits;

  {
    Timestep timestep(absl::UnixEpoch(), absl::Hours(24));
    visit_generator.GenerateVisits(timestep, *risk_score, &visits);
    ASSERT_EQ(3, visits.size());
    for (int i = 0; i < 2; ++i) {
      EXPECT_EQ(visits[i].end_time, visits[i + 1].start_time);
      EXPECT_EQ(i, visits[i].location_uuid);
      EXPECT_GT(VisitDuration(visits[i]), absl::Hours(0));
    }
    EXPECT_EQ(2, visits[2].location_uuid);
    EXPECT_GT(VisitDuration(visits[2]), absl::Hours(0));
    EXPECT_EQ(absl::FromUnixSeconds(86400LL), visits[2].end_time);
  }

  {
    Timestep timestep(absl::UnixEpoch() + absl::Hours(24), absl::Hours(12));
    visit_generator.GenerateVisits(timestep, *risk_score, &visits);
    ASSERT_EQ(6, visits.size());
    EXPECT_EQ(absl::FromUnixSeconds(86400LL), visits[3].start_time);
    EXPECT_EQ(absl::FromUnixSeconds(129600LL), visits[5].end_time);
  }
}

}  // namespace
}  // namespace abesim
