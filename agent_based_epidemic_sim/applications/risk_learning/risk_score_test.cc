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

#include "agent_based_epidemic_sim/applications/risk_learning/risk_score.h"

#include <vector>

#include "absl/time/time.h"
#include "agent_based_epidemic_sim/applications/risk_learning/config.pb.h"
#include "agent_based_epidemic_sim/core/location_type.h"
#include "agent_based_epidemic_sim/core/parse_text_proto.h"
#include "agent_based_epidemic_sim/core/risk_score.h"
#include "agent_based_epidemic_sim/core/timestep.h"
#include "agent_based_epidemic_sim/port/status_matchers.h"
#include "agent_based_epidemic_sim/util/ostream_overload.h"
#include "agent_based_epidemic_sim/util/test_util.h"
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
                                        const LocationReference::Type type) {
  int64 location_uuid = type == LocationReference::BUSINESS ? 0 : 1;

  auto exposure = exposures.begin();
  std::vector<float> adjustments;
  for (const int day : {1, 3, 5, 10, 15, 20, 25}) {
    Timestep timestep(TimeFromDay(day), absl::Hours(24));
    while (exposure != exposures.end() &&
           timestep.end_time() > exposure->start_time) {
      risk_score.AddExposureNotification(
          *exposure, {.test_result = {.outcome = TestOutcome::POSITIVE}});
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
    auto risk_score_model_or =
        CreateLearningRiskScoreModel(GetLearningRiskScoreModelProto());
    return GetRiskScore(risk_score_model_or.value());
  }

  std::unique_ptr<RiskScore> GetRiskScore(const LearningRiskScoreModel& model) {
    model_ = model;
    auto risk_score_policy_or =
        CreateLearningRiskScorePolicy(GetLearningRiskScorePolicyProto());
    policy_ = risk_score_policy_or.value();
    auto risk_score_or = CreateLearningRiskScore(
        GetTracingPolicyProto(), model_, policy_,
        [](const int64 location_uuid) {
          return location_uuid == 0 ? LocationReference::BUSINESS
                                    : LocationReference::HOUSEHOLD;
        });
    return std::move(risk_score_or.value());
  }

  LearningRiskScoreModelProto GetLearningRiskScoreModelProto() {
    return ParseTextProtoOrDie<LearningRiskScoreModelProto>(R"(
      ble_buckets: { weight: 0.1 }
      ble_buckets: { weight: 0.2 max_attenuation: 1 }
      infectiousness_buckets: {
        level: 3
        weight: 0.3
        days_since_symptom_onset_min: -1
        days_since_symptom_onset_max: 1
      }
      infectiousness_buckets: {
        level: 2
        weight: 0.2
        days_since_symptom_onset_min: -2
        days_since_symptom_onset_max: 2
      }
      infectiousness_buckets: {
        level: 1
        weight: 0.1
        days_since_symptom_onset_min: -999
        days_since_symptom_onset_max: 999
      }
    )");
  }

  LearningRiskScorePolicyProto GetLearningRiskScorePolicyProto() {
    return ParseTextProtoOrDie<LearningRiskScorePolicyProto>(R"(
      risk_scale_factor: 1
      exposure_notification_window_days: 3
    )");
  }

  TracingPolicyProto GetTracingPolicyProto() {
    return ParseTextProtoOrDie<TracingPolicyProto>(R"(
      test_validity_duration { seconds: 604800 }
      contact_retention_duration { seconds: 1209600 }
      quarantine_duration { seconds: 1209600 }
      test_latency { seconds: 86400 }
    )");
  }

  LearningRiskScoreModel model_;
  LearningRiskScorePolicy policy_;
};

OVERLOAD_VECTOR_OSTREAM_OPS

struct Case {
  HealthState::State initial_health_state;
  std::vector<Exposure> positive_exposures;
  LocationReference::Type location_type;
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
          .location_type = LocationReference::BUSINESS,
          .expected_adjustments = {1.0, 0.0, 0.0, 0.0, 0.0, 1.0, 1.0},
      },
      {
          .initial_health_state = HealthState::SUSCEPTIBLE,
          .positive_exposures = {},
          .location_type = LocationReference::BUSINESS,
          .expected_adjustments = {1.0, 1.0, 1.0, 1.0, 1.0, 1.0, 1.0},
      },
      // We always go to home locations.
      {
          .initial_health_state = HealthState::SUSCEPTIBLE,
          .positive_exposures = {{.start_time = TimeFromDay(1)}},
          .location_type = LocationReference::HOUSEHOLD,
          .expected_adjustments = {1.0, 1.0, 1.0, 1.0, 1.0, 1.0, 1.0},
      },
      {
          .initial_health_state = HealthState::EXPOSED,
          .positive_exposures = {},
          .location_type = LocationReference::HOUSEHOLD,
          .expected_adjustments = {1.0, 1.0, 1.0, 1.0, 1.0, 1.0, 1.0},
      },
      {
          .initial_health_state = HealthState::EXPOSED,
          .positive_exposures = {{.start_time = TimeFromDay(1)}},
          .location_type = LocationReference::HOUSEHOLD,
          .expected_adjustments = {1.0, 1.0, 1.0, 1.0, 1.0, 1.0, 1.0},
      },
      {
          .initial_health_state = HealthState::SUSCEPTIBLE,
          .positive_exposures = {},
          .location_type = LocationReference::HOUSEHOLD,
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

TEST_F(RiskScoreTest, GetTestResult) {
  auto risk_score = GetRiskScore();

  {
    // Before there is a test we return the null result.
    TestResult result =
        risk_score->GetTestResult(Timestep(TimeFromDay(1), absl::Hours(24)));
    TestResult expected = {.time_requested = absl::InfiniteFuture(),
                           .time_received = absl::InfiniteFuture(),
                           .outcome = TestOutcome::UNSPECIFIED_TEST_RESULT};
    EXPECT_EQ(result, expected);
  }

  risk_score->AddExposureNotification({.start_time = TimeFromDay(1)},
                                      {.test_result = {
                                           .time_requested = TimeFromDay(1),
                                           .time_received = TimeFromDay(2),
                                           .outcome = TestOutcome::NEGATIVE,
                                       }});
  {
    // Negative results don't matter.
    TestResult result =
        risk_score->GetTestResult(Timestep(TimeFromDay(2), absl::Hours(24)));
    TestResult expected = {.time_requested = absl::InfiniteFuture(),
                           .time_received = absl::InfiniteFuture(),
                           .outcome = TestOutcome::UNSPECIFIED_TEST_RESULT};
    EXPECT_EQ(result, expected);
  }

  risk_score->AddExposureNotification({.start_time = TimeFromDay(2)},
                                      {.test_result = {
                                           .time_requested = TimeFromDay(2),
                                           .time_received = TimeFromDay(3),
                                           .outcome = TestOutcome::POSITIVE,
                                       }});
  {
    // On positive contact reports we perform a test, but if we're not sick
    // the result is negative.
    TestResult result =
        risk_score->GetTestResult(Timestep(TimeFromDay(4), absl::Hours(24)));
    TestResult expected = {.time_requested = TimeFromDay(3),
                           .time_received = TimeFromDay(4),
                           .outcome = TestOutcome::NEGATIVE};
    EXPECT_EQ(result, expected);
  }

  risk_score->AddExposureNotification({.start_time = TimeFromDay(8)},
                                      {.test_result = {
                                           .time_requested = TimeFromDay(8),
                                           .time_received = TimeFromDay(9),
                                           .outcome = TestOutcome::POSITIVE,
                                       }});
  {
    // Another positive contact that is within the test validity period will
    // NOT cause another test.
    TestResult result =
        risk_score->GetTestResult(Timestep(TimeFromDay(10), absl::Hours(24)));
    TestResult expected = {.time_requested = TimeFromDay(3),
                           .time_received = TimeFromDay(4),
                           .outcome = TestOutcome::NEGATIVE};
    EXPECT_EQ(result, expected);
  }

  risk_score->AddHealthStateTransistion(
      {.time = TimeFromDay(12), .health_state = HealthState::EXPOSED});
  risk_score->AddExposureNotification({.start_time = TimeFromDay(12)},
                                      {.test_result = {
                                           .time_requested = TimeFromDay(12),
                                           .time_received = TimeFromDay(13),
                                           .outcome = TestOutcome::POSITIVE,
                                       }});
  {
    // Another positive contact after the validity period expires will perform
    // another test.  This time it will report that we are sick since we
    // have transitioned health states.
    TestResult result =
        risk_score->GetTestResult(Timestep(TimeFromDay(14), absl::Hours(24)));
    TestResult expected = {.time_requested = TimeFromDay(13),
                           .time_received = TimeFromDay(14),
                           .outcome = TestOutcome::POSITIVE};
    EXPECT_EQ(result, expected);
  }
}

TEST_F(RiskScoreTest, GetsContactTracingPolicy) {
  auto risk_score = GetRiskScore();

  // If there's no positive test, we don't send.
  EXPECT_THAT(risk_score->GetContactTracingPolicy(
                  Timestep(TimeFromDay(5), absl::Hours(24))),
              Eq(RiskScore::ContactTracingPolicy{.report_recursively = false,
                                                 .send_report = false}));

  risk_score->AddHealthStateTransistion(
      {.time = TimeFromDay(2), .health_state = HealthState::EXPOSED});
  risk_score->AddExposureNotification({}, {.test_result = {
                                               .time_requested = TimeFromDay(3),
                                               .time_received = TimeFromDay(6),
                                               .outcome = TestOutcome::POSITIVE,
                                           }});

  // If the test isn't received yet (will be received on day 7) don't send.
  EXPECT_THAT(risk_score->GetContactTracingPolicy(
                  Timestep(TimeFromDay(5), absl::Hours(24))),
              Eq(RiskScore::ContactTracingPolicy{.report_recursively = false,
                                                 .send_report = false}));
  // The test has been received.
  EXPECT_THAT(risk_score->GetContactTracingPolicy(
                  Timestep(TimeFromDay(7), absl::Hours(24))),
              Eq(RiskScore::ContactTracingPolicy{.report_recursively = false,
                                                 .send_report = true}));
  // Don't send old tests that were requested 2 weeks ago+.
  EXPECT_THAT(risk_score->GetContactTracingPolicy(
                  Timestep(TimeFromDay(21), absl::Hours(24))),
              Eq(RiskScore::ContactTracingPolicy{.report_recursively = false,
                                                 .send_report = false}));
}

TEST_F(RiskScoreTest, GetsContactRetentionDuration) {
  auto risk_score = GetRiskScore();
  EXPECT_EQ(risk_score->ContactRetentionDuration(), absl::Hours(24 * 14));
}

TEST_F(RiskScoreTest, AppEnabledRiskScoreTogglesBehaviorOn) {
  auto risk_score = absl::make_unique<MockRiskScore>();
  EXPECT_CALL(*risk_score, AddExposures).Times(1);
  EXPECT_CALL(*risk_score, AddExposureNotification).Times(1);
  EXPECT_CALL(*risk_score, GetContactTracingPolicy)
      .WillOnce(testing::Return(RiskScore::ContactTracingPolicy{
          .report_recursively = false, .send_report = true}));
  auto app_enabled_risk_score = CreateAppEnabledRiskScore(
      /*is_app_enabled=*/true, std::move(risk_score));
  app_enabled_risk_score->AddExposures(
      Timestep(TimeFromDay(-1), absl::Hours(24)), {});
  app_enabled_risk_score->AddExposureNotification({}, {});
  EXPECT_THAT(app_enabled_risk_score->GetContactTracingPolicy(
                  Timestep(TimeFromDay(21), absl::Hours(24))),
              Eq(RiskScore::ContactTracingPolicy{.report_recursively = false,
                                                 .send_report = true}));
}

TEST_F(RiskScoreTest, AppEnabledRiskScoreTogglesBehaviorOff) {
  auto risk_score = absl::make_unique<MockRiskScore>();
  EXPECT_CALL(*risk_score, AddExposures).Times(0);
  EXPECT_CALL(*risk_score, AddExposureNotification).Times(0);
  EXPECT_CALL(*risk_score, GetContactTracingPolicy).Times(0);
  auto app_enabled_risk_score = CreateAppEnabledRiskScore(
      /*is_app_enabled=*/false, std::move(risk_score));
  app_enabled_risk_score->AddExposures(
      Timestep(TimeFromDay(-1), absl::Hours(24)), {});
  app_enabled_risk_score->AddExposureNotification({}, {});
  EXPECT_THAT(app_enabled_risk_score->GetContactTracingPolicy(
                  Timestep(TimeFromDay(21), absl::Hours(24))),
              Eq(RiskScore::ContactTracingPolicy{.report_recursively = false,
                                                 .send_report = false}));
}

}  // namespace
}  // namespace abesim
