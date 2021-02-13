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

#include <memory>
#include <utility>
#include <vector>

#include "absl/flags/declare.h"
#include "absl/flags/flag.h"
#include "absl/memory/memory.h"
#include "absl/status/statusor.h"
#include "absl/strings/str_format.h"
#include "absl/time/time.h"
#include "agent_based_epidemic_sim/applications/risk_learning/config.pb.h"
#include "agent_based_epidemic_sim/applications/risk_learning/hazard_transmission_model.h"
#include "agent_based_epidemic_sim/core/location_type.h"
#include "agent_based_epidemic_sim/core/pandemic.pb.h"
#include "agent_based_epidemic_sim/core/parse_text_proto.h"
#include "agent_based_epidemic_sim/core/risk_score.h"
#include "agent_based_epidemic_sim/core/risk_score_model.h"
#include "agent_based_epidemic_sim/core/timestep.h"
#include "agent_based_epidemic_sim/port/status_matchers.h"
#include "agent_based_epidemic_sim/util/ostream_overload.h"
#include "agent_based_epidemic_sim/util/test_util.h"
#include "gmock/gmock.h"
#include "gtest/gtest.h"

ABSL_DECLARE_FLAG(bool, request_test_using_hazard);

namespace abesim {
namespace {

using testing::Eq;
using testing::Return;

constexpr char kContacts[] = "contacts";
constexpr char kPositive[] = "positive";

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
    risk_score.UpdateLatestTimestep(timestep);
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
  void InitializeRiskScoreFromModel(
      const TracingPolicyProto& tracing_policy_proto,
      const RiskScoreModel* risk_score_model) {
    auto risk_score_policy_or =
        CreateLearningRiskScorePolicy(GetLearningRiskScorePolicyProto());
    policy_ = *risk_score_policy_or;
    auto risk_score_or = CreateLearningRiskScore(
        tracing_policy_proto, policy_, risk_score_model,
        [](const int64 location_uuid) {
          return location_uuid == 0 ? LocationReference::BUSINESS
                                    : LocationReference::HOUSEHOLD;
        });
    risk_score_ = std::move(*risk_score_or);
  }

  LearningRiskScorePolicyProto GetLearningRiskScorePolicyProto() {
    return ParseTextProtoOrDie<LearningRiskScorePolicyProto>(R"(
      risk_scale_factor: 1
      exposure_notification_window_days: 3
    )");
  }

  TracingPolicyProto GetTracingPolicyProto(
      const bool test_on_symptoms, const float traceable_interaction_fraction,
      const std::string& quarantine_type) {
    return ParseTextProtoOrDie<TracingPolicyProto>(absl::StrFormat(
        R"(
      test_validity_duration { seconds: 604800 }
      contact_retention_duration { seconds: 1209600 }
      quarantine_duration_%s { seconds: 1209600 }
      test_properties {
        sensitivity: 1.0
        specificity: 1.0
        latency { seconds: 86400 }
      }
      test_on_symptoms: %d
      test_risk_score_threshold: 0.0
      trace_on_positive: true
      traceable_interaction_fraction: %f
    )",
        quarantine_type, test_on_symptoms, traceable_interaction_fraction));
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

  std::unique_ptr<RiskScore> risk_score_;
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
  auto risk_score_model =
      CreateLearningRiskScoreModel(GetLearningRiskScoreModelProto());
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
    InitializeRiskScoreFromModel(
        GetTracingPolicyProto(/*test_on_symptoms=*/false,
                              /*traceable_interaction_fraction=*/1.0,
                              /*quarantine_type=*/kContacts),
        (*risk_score_model).get());
    risk_score_->AddHealthStateTransistion({
        .time = absl::InfinitePast(),
        .health_state = c.initial_health_state,
    });
    auto adjustments = FrequencyAdjustments(*risk_score_, c.positive_exposures,
                                            c.location_type);
    EXPECT_THAT(adjustments, testing::ElementsAreArray(c.expected_adjustments))
        << c;
  }
}

TEST_F(RiskScoreTest, GetTestResult) {
  auto risk_score_model =
      CreateLearningRiskScoreModel(GetLearningRiskScoreModelProto());
  InitializeRiskScoreFromModel(
      GetTracingPolicyProto(/*test_on_symptoms=*/false,
                            /*traceable_interaction_fraction=*/1.0,
                            /*quarantine_type=*/kContacts),
      (*risk_score_model).get());

  {
    // Before there is a test we return the null result.
    TestResult result =
        risk_score_->GetTestResult(Timestep(TimeFromDay(1), absl::Hours(24)));
    TestResult expected = {.time_requested = absl::InfiniteFuture(),
                           .time_received = absl::InfiniteFuture(),
                           .outcome = TestOutcome::UNSPECIFIED_TEST_RESULT};
    EXPECT_EQ(result, expected);
  }

  Timestep timestep(TimeFromDay(1), absl::Hours(24));
  risk_score_->UpdateLatestTimestep(timestep);
  risk_score_->AddExposureNotification({.start_time = TimeFromDay(1)},
                                       {.test_result = {
                                            .time_requested = TimeFromDay(1),
                                            .time_received = TimeFromDay(2),
                                            .outcome = TestOutcome::NEGATIVE,
                                        }});
  {
    // Negative results don't matter.
    TestResult result =
        risk_score_->GetTestResult(Timestep(TimeFromDay(2), absl::Hours(24)));
    TestResult expected = {.time_requested = absl::InfiniteFuture(),
                           .time_received = absl::InfiniteFuture(),
                           .outcome = TestOutcome::UNSPECIFIED_TEST_RESULT};
    EXPECT_EQ(result, expected);
  }

  timestep = Timestep(TimeFromDay(1), absl::Hours(24));
  risk_score_->AddExposureNotification({.start_time = TimeFromDay(2)},
                                       {.test_result = {
                                            .time_requested = TimeFromDay(2),
                                            .time_received = TimeFromDay(3),
                                            .outcome = TestOutcome::POSITIVE,
                                        }});
  {
    // On positive contact reports we perform a test, but if we're not sick
    // the result is negative.
    TestResult result =
        risk_score_->GetTestResult(Timestep(TimeFromDay(4), absl::Hours(24)));
    TestResult expected = {.time_requested = TimeFromDay(3),
                           .time_received = TimeFromDay(4),
                           .outcome = TestOutcome::NEGATIVE};
    EXPECT_EQ(result, expected);
  }

  risk_score_->AddExposureNotification({.start_time = TimeFromDay(8)},
                                       {.test_result = {
                                            .time_requested = TimeFromDay(8),
                                            .time_received = TimeFromDay(9),
                                            .outcome = TestOutcome::POSITIVE,
                                        }});
  {
    // Another positive contact that is within the test validity period will
    // NOT cause another test.
    TestResult result =
        risk_score_->GetTestResult(Timestep(TimeFromDay(10), absl::Hours(24)));
    TestResult expected = {.time_requested = TimeFromDay(3),
                           .time_received = TimeFromDay(4),
                           .outcome = TestOutcome::NEGATIVE};
    EXPECT_EQ(result, expected);
  }

  risk_score_->AddHealthStateTransistion(
      {.time = TimeFromDay(12), .health_state = HealthState::EXPOSED});
  risk_score_->AddExposureNotification({.start_time = TimeFromDay(12)},
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
        risk_score_->GetTestResult(Timestep(TimeFromDay(14), absl::Hours(24)));
    TestResult expected = {.time_requested = TimeFromDay(13),
                           .time_received = TimeFromDay(14),
                           .outcome = TestOutcome::POSITIVE};
    EXPECT_EQ(result, expected);
  }
}

TEST_F(RiskScoreTest, GetsContactTracingPolicy) {
  auto risk_score_model =
      CreateLearningRiskScoreModel(GetLearningRiskScoreModelProto());
  InitializeRiskScoreFromModel(
      GetTracingPolicyProto(/*test_on_symptoms=*/false,
                            /*traceable_interaction_fraction=*/1.0,
                            /*quarantine_type=*/kContacts),
      (*risk_score_model).get());

  // If there's no positive test, we don't send.
  EXPECT_THAT(risk_score_->GetContactTracingPolicy(
                  Timestep(TimeFromDay(5), absl::Hours(24))),
              Eq(RiskScore::ContactTracingPolicy{.report_recursively = false,
                                                 .send_report = false}));

  risk_score_->AddHealthStateTransistion(
      {.time = TimeFromDay(2), .health_state = HealthState::EXPOSED});
  risk_score_->UpdateLatestTimestep(Timestep(TimeFromDay(3), absl::Hours(24)));
  risk_score_->AddExposureNotification({},
                                       {.test_result = {
                                            .time_requested = TimeFromDay(3),
                                            .time_received = TimeFromDay(6),
                                            .outcome = TestOutcome::POSITIVE,
                                        }});

  // If the test isn't received yet (will be received on day 7) don't send.
  EXPECT_THAT(risk_score_->GetContactTracingPolicy(
                  Timestep(TimeFromDay(5), absl::Hours(24))),
              Eq(RiskScore::ContactTracingPolicy{.report_recursively = false,
                                                 .send_report = false}));
  // The test has been received.
  EXPECT_THAT(risk_score_->GetContactTracingPolicy(
                  Timestep(TimeFromDay(7), absl::Hours(24))),
              Eq(RiskScore::ContactTracingPolicy{.report_recursively = false,
                                                 .send_report = true}));
  // Don't send old tests that were requested 2 weeks ago+.
  EXPECT_THAT(risk_score_->GetContactTracingPolicy(
                  Timestep(TimeFromDay(21), absl::Hours(24))),
              Eq(RiskScore::ContactTracingPolicy{.report_recursively = false,
                                                 .send_report = false}));
}

TEST_F(RiskScoreTest, GetsContactRetentionDuration) {
  auto risk_score_model =
      CreateLearningRiskScoreModel(GetLearningRiskScoreModelProto());
  InitializeRiskScoreFromModel(
      GetTracingPolicyProto(/*test_on_symptoms=*/false,
                            /*traceable_interaction_fraction=*/1.0,
                            /*quarantine_type=*/kContacts),
      (*risk_score_model).get());
  EXPECT_EQ(risk_score_->ContactRetentionDuration(), absl::Hours(24 * 14));
}

TEST_F(RiskScoreTest, AppEnabledRiskScoreTogglesBehaviorOn) {
  auto risk_score = absl::make_unique<MockRiskScore>();
  EXPECT_CALL(*risk_score, AddExposureNotification).Times(1);
  EXPECT_CALL(*risk_score, GetContactTracingPolicy)
      .WillOnce(testing::Return(RiskScore::ContactTracingPolicy{
          .report_recursively = false, .send_report = true}));
  auto app_enabled_risk_score = CreateAppEnabledRiskScore(
      /*is_app_enabled=*/true, std::move(risk_score));
  app_enabled_risk_score->AddExposureNotification({}, {});
  EXPECT_THAT(app_enabled_risk_score->GetContactTracingPolicy(
                  Timestep(TimeFromDay(21), absl::Hours(24))),
              Eq(RiskScore::ContactTracingPolicy{.report_recursively = false,
                                                 .send_report = true}));
}

TEST_F(RiskScoreTest, AppEnabledRiskScoreTogglesBehaviorOff) {
  auto risk_score = absl::make_unique<MockRiskScore>();
  EXPECT_CALL(*risk_score, AddExposureNotification).Times(0);
  EXPECT_CALL(*risk_score, GetContactTracingPolicy).Times(0);
  auto app_enabled_risk_score = CreateAppEnabledRiskScore(
      /*is_app_enabled=*/false, std::move(risk_score));
  app_enabled_risk_score->AddExposureNotification({}, {});
  EXPECT_THAT(app_enabled_risk_score->GetContactTracingPolicy(
                  Timestep(TimeFromDay(21), absl::Hours(24))),
              Eq(RiskScore::ContactTracingPolicy{.report_recursively = false,
                                                 .send_report = false}));
}

TEST_F(RiskScoreTest, HazardQueryingRiskScoreAppendsHazard) {
  absl::SetFlag(&FLAGS_request_test_using_hazard, true);
  auto risk_score = absl::make_unique<MockRiskScore>();
  auto mock_risk_score = risk_score.get();
  auto request_time = absl::UnixEpoch() + absl::Hours(24);
  Timestep timestep(request_time, absl::Hours(24));
  auto hazard = absl::make_unique<Hazard>();
  std::vector<Exposure> exposures{{
      .start_time = absl::UnixEpoch(),
      .duration = absl::Hours(48),
      .distance = 0,
      .infectivity = 1,
      .symptom_factor = 1,
      .susceptibility = 1,
      .location_transmissibility = 1,
  }};
  hazard->GetTransmissionModel()->GetInfectionOutcome(MakePointers(exposures));
  auto hazard_risk_score =
      CreateHazardQueryingRiskScore(std::move(hazard), std::move(risk_score));
  {
    testing::InSequence seq;
    EXPECT_CALL(*mock_risk_score, GetTestResult(Eq(timestep)))
        .WillOnce(Return(TestResult{.time_requested = absl::InfiniteFuture()}));
    EXPECT_CALL(*mock_risk_score, RequestTest(Eq(request_time)));
    EXPECT_CALL(*mock_risk_score, GetTestResult(Eq(timestep)))
        .WillOnce(Return(TestResult{.time_requested = request_time}));
  }
  auto result = hazard_risk_score->GetTestResult(timestep);
  EXPECT_GT(result.hazard, 0);
  EXPECT_EQ(request_time, result.time_requested);
}

TEST_F(RiskScoreTest, GetRiskScoreCountsCorrectly) {
  auto risk_score_model = absl::make_unique<MockRiskScoreModel>();
  InitializeRiskScoreFromModel(
      GetTracingPolicyProto(/*test_on_symptoms=*/false,
                            /*traceable_interaction_fraction=*/1.0,
                            /*quarantine_type=*/kContacts),
      risk_score_model.get());

  Timestep timestep(TimeFromDay(1), absl::Hours(24));
  // risk_score->GetRiskScore returns the sum of risk scores in the first (and
  // last) timestep.
  {
    EXPECT_CALL(*risk_score_model, ComputeRiskScore)
        .Times(2)
        .WillRepeatedly(Return(1));

    // Always call UpdateLatestTimestep to initialize the risk score buffer.
    risk_score_->UpdateLatestTimestep(timestep);
    risk_score_->AddExposureNotification(
        {
            .start_time = TimeFromDayAndHour(1, 1),
        },
        {
            .test_result = {.outcome = TestOutcome::POSITIVE},
        });

    EXPECT_THAT(risk_score_->GetRiskScore(), Eq(1));

    risk_score_->AddExposureNotification(
        {
            .start_time = TimeFromDayAndHour(1, 2),
        },
        {
            .test_result = {.outcome = TestOutcome::POSITIVE},
        });

    EXPECT_THAT(risk_score_->GetRiskScore(), Eq(2));
  }

  timestep = Timestep(TimeFromDay(2), absl::Hours(24));
  // risk_score->GetRiskScore returns the sum of risk scores in the last two
  // timesteps.
  {
    EXPECT_CALL(*risk_score_model, ComputeRiskScore).WillOnce(Return(2));

    // Always call UpdateLatestTimestep to initialize the risk score buffer.
    risk_score_->UpdateLatestTimestep(timestep);
    risk_score_->AddExposureNotification(
        {
            .start_time = TimeFromDayAndHour(2, 1),
        },
        {
            .test_result = {.outcome = TestOutcome::POSITIVE},
        });

    EXPECT_THAT(risk_score_->GetRiskScore(), Eq(4));
  }

  timestep = Timestep(TimeFromDay(3), absl::Hours(24));
  // risk_score->GetRiskScore returns the sum of risk scores in the last three
  // timesteps. Note: exposure_notification_window_days == 3 in this test setup.
  {
    EXPECT_CALL(*risk_score_model, ComputeRiskScore).WillOnce(Return(3));

    // Always call UpdateLatestTimestep to initialize the risk score buffer.
    risk_score_->UpdateLatestTimestep(timestep);
    risk_score_->AddExposureNotification(
        {
            .start_time = TimeFromDayAndHour(3, 1),
        },
        {
            .test_result = {.outcome = TestOutcome::POSITIVE},
        });

    EXPECT_THAT(risk_score_->GetRiskScore(), Eq(7));
  }

  // risk_score->GetRiskScore returns the sum of risk scores not including
  // those outside of exposure_notification_window_days.
  {
    EXPECT_CALL(*risk_score_model, ComputeRiskScore).WillOnce(Return(5));

    // Always call UpdateLatestTimestep to initialize the risk score buffer.
    for (int i = 4; i < 19; ++i) {
      timestep = Timestep(TimeFromDay(i), absl::Hours(24));
      risk_score_->UpdateLatestTimestep(timestep);
    }
    risk_score_->AddExposureNotification(
        {
            .start_time = TimeFromDayAndHour(15, 1),
        },
        {
            .test_result = {.outcome = TestOutcome::POSITIVE},
        });

    EXPECT_THAT(risk_score_->GetRiskScore(), Eq(5));
  }
}

TEST_F(RiskScoreTest, TestsOnSymptoms) {
  auto risk_score_model = absl::make_unique<MockRiskScoreModel>();
  InitializeRiskScoreFromModel(
      GetTracingPolicyProto(/*test_on_symptoms=*/true,
                            /*traceable_interaction_fraction=*/1.0,
                            /*quarantine_type=*/kContacts),
      risk_score_model.get());
  risk_score_->AddHealthStateTransistion(
      {.time = TimeFromDay(2),
       .health_state = HealthState::SYMPTOMATIC_SEVERE});
  TestResult expected = {.time_requested = TimeFromDay(2),
                         .time_received = TimeFromDay(3),
                         .outcome = TestOutcome::POSITIVE};
  {
    TestResult result =
        risk_score_->GetTestResult(Timestep(TimeFromDay(5), absl::Hours(24)));
    EXPECT_EQ(result, expected);
  }
  risk_score_->AddHealthStateTransistion(
      {.time = TimeFromDay(10),
       .health_state = HealthState::SYMPTOMATIC_HOSPITALIZED_RECOVERING});
  {
    TestResult result =
        risk_score_->GetTestResult(Timestep(TimeFromDay(10), absl::Hours(24)));
    EXPECT_EQ(result, expected);
  }
}

TEST_F(RiskScoreTest, QuarantinesOnPositive) {
  auto risk_score_model = absl::make_unique<MockRiskScoreModel>();
  InitializeRiskScoreFromModel(
      GetTracingPolicyProto(/*test_on_symptoms=*/true,
                            /*traceable_interaction_fraction=*/1.0,
                            /*quarantine_type=*/kPositive),
      risk_score_model.get());
  risk_score_->AddHealthStateTransistion(
      {.time = TimeFromDay(2),
       .health_state = HealthState::SYMPTOMATIC_SEVERE});
  EXPECT_EQ(1.0,
            risk_score_
                ->GetVisitAdjustment(Timestep(TimeFromDay(2), absl::Hours(24)),
                                     /*location_uuid=*/0)
                .frequency_adjustment);
  EXPECT_EQ(0.0,
            risk_score_
                ->GetVisitAdjustment(Timestep(TimeFromDay(5), absl::Hours(24)),
                                     /*location_uuid=*/0)
                .frequency_adjustment);
}

TEST_F(RiskScoreTest, DropsIfNoTraceableInteractionFraction) {
  auto risk_score_model = absl::make_unique<MockRiskScoreModel>();
  EXPECT_CALL(*risk_score_model, ComputeRiskScore).Times(0);
  InitializeRiskScoreFromModel(GetTracingPolicyProto(
                                   /*test_on_symptoms=*/false,
                                   /*traceable_interaction_fraction=*/0.0,
                                   /*quarantine_type=*/kContacts),
                               risk_score_model.get());
  Timestep timestep(TimeFromDay(1), absl::Hours(24));
  risk_score_->AddExposureNotification({.start_time = TimeFromDay(1)},
                                       {.test_result = {
                                            .time_requested = TimeFromDay(1),
                                            .time_received = TimeFromDay(2),
                                            .outcome = TestOutcome::POSITIVE,
                                        }});
}

}  // namespace
}  // namespace abesim
