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

#include <initializer_list>

#include "absl/time/time.h"
#include "agent_based_epidemic_sim/core/integral_types.h"
#include "agent_based_epidemic_sim/core/risk_score.h"
#include "agent_based_epidemic_sim/core/visit.h"
#include "gmock/gmock.h"
#include "gtest/gtest.h"

namespace abesim {
namespace {

using testing::Return;

absl::Duration VisitDuration(const Visit& visit) {
  return visit.end_time - visit.start_time;
}

std::vector<LocationDuration> MakeLocationDurationVector(
    const std::vector<float>& durations) {
  std::vector<LocationDuration> location_duration;
  for (int i = 0; i < durations.size(); ++i) {
    location_duration.push_back({
        .location_uuid = i,
        .sample_duration =
            [&durations, i](float adjustment) {
              return durations[i] * adjustment;
            },
    });
  }
  return location_duration;
}

TEST(DurationSpecifiedVisitGeneratorTest, GeneratesVisits) {
  std::vector<float> durations{8, 6, 2};
  std::vector<LocationDuration> location_duration =
      MakeLocationDurationVector(durations);
  DurationSpecifiedVisitGenerator visit_generator({location_duration});
  auto risk_score = NewNullRiskScore();

  std::vector<Visit> visits;
  {
    Timestep timestep(absl::UnixEpoch(), absl::Hours(24));
    visit_generator.GenerateVisits(timestep, *risk_score, &visits);
    ASSERT_EQ(3, visits.size());
    for (int i = 0; i < 2; ++i) {
      EXPECT_EQ(visits[i].end_time, visits[i + 1].start_time);
      EXPECT_EQ(i, visits[i].location_uuid);
      EXPECT_EQ(absl::Hours(1.5 * durations[i]), VisitDuration(visits[i]));
      EXPECT_LE(visits[i].end_time, timestep.end_time());
    }
    EXPECT_EQ(2, visits[2].location_uuid);
    EXPECT_EQ(absl::Hours(1.5 * durations[2]), VisitDuration(visits[2]));
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

TEST(DurationSpecifiedVisitGeneratorTest,
     GeneratesVisitsWithAllZeroDurationSamples) {
  std::vector<float> durations{0, 0, 0};
  std::vector<LocationDuration> location_duration =
      MakeLocationDurationVector(durations);
  DurationSpecifiedVisitGenerator visit_generator({location_duration});
  auto risk_score = NewNullRiskScore();

  std::vector<Visit> visits;
  Timestep timestep(absl::UnixEpoch(), absl::Hours(24));
  visit_generator.GenerateVisits(timestep, *risk_score, &visits);
  ASSERT_EQ(1, visits.size());
  EXPECT_EQ(visits[0].start_time, timestep.start_time());
  EXPECT_EQ(visits[0].end_time, timestep.end_time());
}

TEST(DurationSpecifiedVisitGeneratorTest, GeneratesVisitsWithNegativeSamples) {
  std::vector<float> durations{-1, 3, -1};
  std::vector<LocationDuration> location_duration =
      MakeLocationDurationVector(durations);
  DurationSpecifiedVisitGenerator visit_generator({location_duration});
  auto risk_score = NewNullRiskScore();

  std::vector<Visit> visits;
  Timestep timestep(absl::UnixEpoch(), absl::Hours(24));
  visit_generator.GenerateVisits(timestep, *risk_score, &visits);
  ASSERT_EQ(1, visits.size());
  EXPECT_EQ(visits[0].start_time, timestep.start_time());
  EXPECT_EQ(visits[0].end_time, timestep.end_time());
}

class MockRiskScore : public RiskScore {
 public:
  MOCK_METHOD(void, AddHealthStateTransistion, (HealthTransition transition),
              (override));
  MOCK_METHOD(void, AddExposures, (absl::Span<const Exposure* const> exposures),
              (override));
  MOCK_METHOD(void, AddExposureNotification,
              (const Contact& contact, const TestResult& result), (override));
  MOCK_METHOD(VisitAdjustment, GetVisitAdjustment,
              (const Timestep& timestep, int64 location_uuid),
              (const, override));
  MOCK_METHOD(TestResult, GetTestResult, (const Timestep& timestep),
              (const override));
  MOCK_METHOD(ContactTracingPolicy, GetContactTracingPolicy,
              (const Timestep& timestep), (const, override));
  MOCK_METHOD(absl::Duration, ContactRetentionDuration, (), (const, override));
};

MATCHER_P(TimeNear, a, "") {
  return arg >= a - absl::Seconds(1) && arg <= a + absl::Seconds(1);
}

TEST(DurationSpecifiedVisitGeneratorTest, GeneratesFrequencyAdjustedVisits) {
  std::vector<float> durations{1, 9, 23};
  std::vector<LocationDuration> location_duration =
      MakeLocationDurationVector(durations);
  DurationSpecifiedVisitGenerator visit_generator({location_duration});
  MockRiskScore mock_risk_score;

  Timestep timestep(absl::UnixEpoch(), absl::Hours(24));
  // Currently there isn't a nice way to mock the random number generator inside
  // the visit generator, so we will choose either 1.0 or 0.0 here.
  std::vector<float> frequency_adjustments = {1.0, 0.0, 1.0};
  for (int i = 0; i < frequency_adjustments.size(); ++i) {
    EXPECT_CALL(mock_risk_score, GetVisitAdjustment(timestep, i))
        .WillOnce(Return(RiskScore::VisitAdjustment{
            .frequency_adjustment = frequency_adjustments[i],
            .duration_adjustment = 1.0,
        }));
  }

  std::vector<Visit> visits;
  visit_generator.GenerateVisits(timestep, mock_risk_score, &visits);
  ASSERT_EQ(2, visits.size());
  EXPECT_EQ(0, visits[0].location_uuid);
  EXPECT_EQ(timestep.start_time(), visits[0].start_time);
  EXPECT_THAT(visits[0].end_time,
              TimeNear(timestep.start_time() + absl::Hours(1)));
  EXPECT_EQ(2, visits[1].location_uuid);
  EXPECT_THAT(visits[1].start_time,
              TimeNear(timestep.start_time() + absl::Hours(1)));
  EXPECT_EQ(timestep.end_time(), visits[1].end_time);
}

TEST(DurationSpecifiedVisitGeneratorTest, GeneratesDurationAdjustedVisits) {
  std::vector<float> durations{3, 3, 3};
  std::vector<LocationDuration> location_duration =
      MakeLocationDurationVector(durations);
  DurationSpecifiedVisitGenerator visit_generator({location_duration});
  MockRiskScore mock_risk_score;

  Timestep timestep(absl::UnixEpoch(), absl::Hours(24));
  // Currently there isn't a nice way to mock the random number generator inside
  // the visit generator, so we will choose either 1.0 or 0.0 here.
  std::vector<float> duration_adjustments = {0.0, 1.0 / 3.0, 2.0 / 3.0};
  for (int i = 0; i < duration_adjustments.size(); ++i) {
    EXPECT_CALL(mock_risk_score, GetVisitAdjustment(timestep, i))
        .WillOnce(Return(RiskScore::VisitAdjustment{
            .frequency_adjustment = 1.0,
            .duration_adjustment = duration_adjustments[i],
        }));
  }

  std::vector<Visit> visits;
  visit_generator.GenerateVisits(timestep, mock_risk_score, &visits);
  ASSERT_EQ(2, visits.size());
  EXPECT_EQ(1, visits[0].location_uuid);
  EXPECT_EQ(timestep.start_time(), visits[0].start_time);
  EXPECT_THAT(visits[0].end_time,
              TimeNear(timestep.start_time() + absl::Hours(8)));
  EXPECT_EQ(2, visits[1].location_uuid);
  EXPECT_THAT(visits[1].start_time,
              TimeNear(timestep.start_time() + absl::Hours(8)));
  EXPECT_EQ(timestep.end_time(), visits[1].end_time);
}

}  // namespace
}  // namespace abesim
