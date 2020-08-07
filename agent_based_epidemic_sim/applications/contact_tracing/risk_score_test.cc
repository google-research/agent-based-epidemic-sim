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

#include "agent_based_epidemic_sim/applications/contact_tracing/risk_score.h"

#include <vector>

#include "absl/time/time.h"
#include "agent_based_epidemic_sim/applications/contact_tracing/config.pb.h"
#include "agent_based_epidemic_sim/applications/home_work/location_type.h"
#include "agent_based_epidemic_sim/core/parse_text_proto.h"
#include "agent_based_epidemic_sim/core/risk_score.h"
#include "agent_based_epidemic_sim/port/status_matchers.h"
#include "agent_based_epidemic_sim/util/ostream_overload.h"
#include "gmock/gmock.h"
#include "gtest/gtest.h"

namespace abesim {
namespace {

using testing::Eq;

absl::Time TimeFromDayAndHour(const int day, const int hour) {
  return absl::UnixEpoch() + absl::Hours(24 * day + hour);
}
absl::Time TimeFromDay(const int day) { return TimeFromDayAndHour(day, 0); }

std::vector<float> FrequencyAdjustments(RiskScore& risk_score,
                                        absl::Span<const Exposure> exposures,
                                        const LocationType type) {
  int64 location_uuid = type == LocationType::kWork ? 0 : 1;

  auto exposure = exposures.begin();
  std::vector<float> adjustments;
  for (const int day : {1, 3, 5, 10, 15, 20, 25}) {
    Timestep timestep(TimeFromDay(day), absl::Hours(24));
    while (exposure != exposures.end() &&
           timestep.end_time() > exposure->start_time) {
      risk_score.AddExposureNotification({.exposure = *exposure}, {});
      exposure++;
    }
    adjustments.push_back(risk_score.GetVisitAdjustment(timestep, location_uuid)
                              .frequency_adjustment);
  }
  return adjustments;
}

class RiskScoreTest : public testing::Test {
 protected:
  std::unique_ptr<RiskScore> GetRiskScore() {
    auto risk_score_or = CreateTracingRiskScore(
        GetTracingPolicyProto(), [](const int64 location_uuid) {
          return location_uuid == 0 ? LocationType::kWork : LocationType::kHome;
        });
    return std::move(risk_score_or.value());
  }

 private:
  TracingPolicyProto GetTracingPolicyProto() {
    return ParseTextProtoOrDie<TracingPolicyProto>(R"(
      test_validity_duration { seconds: 604800 }
      contact_retention_duration { seconds: 1209600 }
      quarantine_duration { seconds: 1209600 }
      test_latency { seconds: 86400 }
      positive_threshold: .9
    )");
  }
};

OVERLOAD_VECTOR_OSTREAM_OPS

struct Case {
  HealthState::State initial_health_state;
  std::vector<Exposure> positive_exposures;
  LocationType location_type;
  std::vector<float> expected_adjustments;

  friend std::ostream& operator<<(std::ostream& strm, const Case& c) {
    return strm << "{" << c.initial_health_state << ", " << c.positive_exposures
                << ", " << static_cast<int>(c.location_type) << ", "
                << c.expected_adjustments << "}";
  }
};

TEST_F(RiskScoreTest, GetVisitAdjustment) {
  Case cases[] = {
      {
          .initial_health_state = HealthState::SUSCEPTIBLE,
          .positive_exposures = {{.start_time = TimeFromDayAndHour(2, 4)}},
          .location_type = LocationType::kWork,
          .expected_adjustments = {1.0, 0.0, 0.0, 0.0, 0.0, 1.0, 1.0},
      },
      {
          .initial_health_state = HealthState::EXPOSED,
          .positive_exposures = {},
          .location_type = LocationType::kWork,
          .expected_adjustments = {0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0},
      },
      {
          .initial_health_state = HealthState::EXPOSED,
          .positive_exposures = {{.start_time = TimeFromDay(1)}},
          .location_type = LocationType::kWork,
          .expected_adjustments = {0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0},
      },
      {
          .initial_health_state = HealthState::SUSCEPTIBLE,
          .positive_exposures = {},
          .location_type = LocationType::kWork,
          .expected_adjustments = {1.0, 1.0, 1.0, 1.0, 1.0, 1.0, 1.0},
      },
      // We always go to home locations.
      {
          .initial_health_state = HealthState::SUSCEPTIBLE,
          .positive_exposures = {{.start_time = TimeFromDay(1)}},
          .location_type = LocationType::kHome,
          .expected_adjustments = {1.0, 1.0, 1.0, 1.0, 1.0, 1.0, 1.0},
      },
      {
          .initial_health_state = HealthState::EXPOSED,
          .positive_exposures = {},
          .location_type = LocationType::kHome,
          .expected_adjustments = {1.0, 1.0, 1.0, 1.0, 1.0, 1.0, 1.0},
      },
      {
          .initial_health_state = HealthState::EXPOSED,
          .positive_exposures = {{.start_time = TimeFromDay(1)}},
          .location_type = LocationType::kHome,
          .expected_adjustments = {1.0, 1.0, 1.0, 1.0, 1.0, 1.0, 1.0},
      },
      {
          .initial_health_state = HealthState::SUSCEPTIBLE,
          .positive_exposures = {},
          .location_type = LocationType::kHome,
          .expected_adjustments = {1.0, 1.0, 1.0, 1.0, 1.0, 1.0, 1.0},
      },
  };

  for (const Case& c : cases) {
    auto risk_score = GetRiskScore();
    risk_score->AddHealthStateTransistion({
        .time = absl::InfinitePast(),
        .health_state = c.initial_health_state,
    });
    auto adjustments = FrequencyAdjustments(*risk_score, c.positive_exposures,
                                            c.location_type);
    EXPECT_THAT(adjustments, testing::ElementsAreArray(c.expected_adjustments))
        << c;
  }
}

TEST_F(RiskScoreTest, GetsTestPolicyUntested) {
  auto risk_score = GetRiskScore();
  risk_score->AddExposureNotification(
      {.exposure = {.start_time = TimeFromDayAndHour(1, 1)}}, {});
  Timestep timestep(TimeFromDay(2), absl::Hours(24));
  EXPECT_THAT(
      risk_score->GetTestPolicy(timestep),
      Eq(RiskScore::TestPolicy{.should_test = true,
                               .time_requested = TimeFromDayAndHour(1, 1),
                               .latency = absl::Hours(24)}));
}

TEST_F(RiskScoreTest, GetsTestPolicyRetry) {
  auto risk_score = GetRiskScore();
  risk_score->AddExposureNotification(
      {.exposure = {.start_time = TimeFromDayAndHour(1, 1)}}, {});
  risk_score->AddTestResult({.time_requested = TimeFromDayAndHour(1, 1),
                             .time_received = absl::InfiniteFuture(),
                             .needs_retry = true,
                             .probability = 0.0f});
  Timestep timestep(TimeFromDay(2), absl::Hours(24));
  EXPECT_THAT(
      risk_score->GetTestPolicy(timestep),
      Eq(RiskScore::TestPolicy{.should_test = true,
                               .time_requested = TimeFromDayAndHour(1, 1),
                               .latency = absl::Hours(24)}));
}

TEST_F(RiskScoreTest, GetsTestPolicyTooSoonForRetest) {
  auto risk_score = GetRiskScore();
  risk_score->AddTestResult({.time_requested = TimeFromDayAndHour(0, 1),
                             .time_received = TimeFromDayAndHour(1, 1),
                             .needs_retry = false,
                             .probability = 0.0f});
  risk_score->AddExposureNotification(
      {.exposure = {.start_time = TimeFromDayAndHour(0, 1)}}, {});
  Timestep timestep(TimeFromDay(2), absl::Hours(24));
  EXPECT_THAT(risk_score->GetTestPolicy(timestep),
              Eq(RiskScore::TestPolicy{.should_test = false}));
}

TEST_F(RiskScoreTest, GetsTestPolicyRetest) {
  auto risk_score = GetRiskScore();
  risk_score->AddTestResult({.time_requested = TimeFromDayAndHour(0, 1),
                             .time_received = TimeFromDayAndHour(1, 0),
                             .needs_retry = false,
                             .probability = 0.0f});
  risk_score->AddExposureNotification(
      {.exposure = {.start_time = TimeFromDayAndHour(11, 1)}}, {});
  Timestep timestep(TimeFromDay(12), absl::Hours(24));
  EXPECT_THAT(
      risk_score->GetTestPolicy(timestep),
      Eq(RiskScore::TestPolicy{.should_test = true,
                               .time_requested = TimeFromDayAndHour(11, 1),
                               .latency = absl::Hours(24)}));
}

TEST_F(RiskScoreTest, GetsTestPolicyNoRetestNeeded) {
  auto risk_score = GetRiskScore();
  risk_score->AddTestResult({.time_requested = TimeFromDayAndHour(0, 1),
                             .time_received = TimeFromDayAndHour(1, 0),
                             .needs_retry = false,
                             .probability = 1.0f});
  risk_score->AddExposureNotification(
      {.exposure = {.start_time = TimeFromDayAndHour(11, 4)}}, {});
  Timestep timestep(TimeFromDay(12), absl::Hours(24));
  EXPECT_THAT(risk_score->GetTestPolicy(timestep),
              Eq(RiskScore::TestPolicy{.should_test = false}));
}

TEST_F(RiskScoreTest, GetsTestPolicyNoRecentContacts) {
  auto risk_score = GetRiskScore();
  risk_score->AddExposureNotification(
      {.exposure = {.start_time = TimeFromDayAndHour(0, 1)}}, {});
  Timestep timestep(TimeFromDay(15), absl::Hours(24));
  EXPECT_THAT(risk_score->GetTestPolicy(timestep),
              Eq(RiskScore::TestPolicy{.should_test = false}));
}

TEST_F(RiskScoreTest, GetsContactTracingPolicy) {
  auto risk_score = GetRiskScore();
  EXPECT_THAT(risk_score->GetContactTracingPolicy(),
              Eq(RiskScore::ContactTracingPolicy{.report_recursively = false,
                                                 .send_positive_test = true}));
}

TEST_F(RiskScoreTest, GetsContactRetentionDuration) {
  auto risk_score = GetRiskScore();
  EXPECT_EQ(risk_score->ContactRetentionDuration(), absl::Hours(24 * 14));
}

}  // namespace
}  // namespace abesim
