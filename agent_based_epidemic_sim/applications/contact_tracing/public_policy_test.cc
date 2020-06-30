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

#include "agent_based_epidemic_sim/applications/contact_tracing/public_policy.h"

#include "absl/time/time.h"
#include "agent_based_epidemic_sim/applications/contact_tracing/config.pb.h"
#include "agent_based_epidemic_sim/core/parse_text_proto.h"
#include "agent_based_epidemic_sim/core/public_policy.h"
#include "agent_based_epidemic_sim/port/status_matchers.h"
#include "agent_based_epidemic_sim/util/ostream_overload.h"
#include "gmock/gmock.h"
#include "gtest/gtest.h"

namespace abesim {
namespace {

using testing::Eq;

absl::Time TestDay(int day) {
  return absl::UnixEpoch() + absl::Hours(24) * day;
}

std::vector<float> FrequencyAdjustments(const PublicPolicy* const policy,
                                        const HealthState::State health_state,
                                        const ContactSummary& contact_summary,
                                        const LocationType type,
                                        const std::vector<int>& days) {
  int64 location_uuid = type == LocationType::kWork ? 0 : 1;

  std::vector<float> adjustments;
  for (const int day : days) {
    Timestep timestep(TestDay(day), absl::Hours(24));
    adjustments.push_back(policy
                              ->GetVisitAdjustment(timestep, health_state,
                                                   contact_summary,
                                                   location_uuid)
                              .frequency_adjustment);
  }
  return adjustments;
}

OVERLOAD_VECTOR_OSTREAM_OPS

struct Case {
  ContactSummary contact_summary;
  HealthState::State health_state;
  LocationType location_type;
  std::vector<float> expected_frequency_adjustments;

  friend std::ostream& operator<<(std::ostream& strm, const Case& c) {
    return strm << "{" << c.contact_summary << ", " << c.health_state << ", "
                << static_cast<int>(c.location_type) << ", "
                << c.expected_frequency_adjustments << "}";
  }
};

class PublicPolicyTest : public testing::Test {
 protected:
  void SetUp() override {
    auto policy_or = CreateTracingPolicy(
        GetTracingPolicyProto(), [](const int64 location_uuid) {
          return location_uuid == 0 ? LocationType::kWork : LocationType::kHome;
        });
    PANDEMIC_ASSERT_OK(policy_or);
    policy_ = std::move(policy_or.value());
  }

  TracingPolicyProto GetTracingPolicyProto() {
    return ParseTextProtoOrDie<TracingPolicyProto>(R"(
      test_validity_duration { seconds: 604800 }
      contact_retention_duration { seconds: 1209600 }
      quarantine_duration { seconds: 1209600 }
      test_latency { seconds: 86400 }
      positive_threshold: .9
    )");
  }

  std::unique_ptr<PublicPolicy> policy_;
};

TEST_F(PublicPolicyTest, GetsVisitAdjustments) {
  std::vector<int> test_days = {1, 3, 5, 10, 15, 20, 25};
  Case cases[] = {
      {{.retention_horizon = absl::FromUnixSeconds(172800LL),
        .latest_contact_time = absl::FromUnixSeconds(200000LL)},
       HealthState::SUSCEPTIBLE,
       LocationType::kWork,
       {1.0, 0.0, 0.0, 0.0, 0.0, 1.0, 1.0}},
      {{.retention_horizon = absl::InfiniteFuture(),
        .latest_contact_time = absl::InfinitePast()},
       HealthState::EXPOSED,
       LocationType::kWork,
       {0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0}},
      {{.retention_horizon = absl::FromUnixSeconds(0LL),
        .latest_contact_time = absl::FromUnixSeconds(86400LL)},
       HealthState::EXPOSED,
       LocationType::kWork,
       {0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0}},
      {{.retention_horizon = absl::InfiniteFuture(),
        .latest_contact_time = absl::InfinitePast()},
       HealthState::SUSCEPTIBLE,
       LocationType::kWork,
       {1.0, 1.0, 1.0, 1.0, 1.0, 1.0, 1.0}},
      // We always go to home locations.
      {{.retention_horizon = absl::FromUnixSeconds(0LL),
        .latest_contact_time = absl::FromUnixSeconds(86400LL)},
       HealthState::SUSCEPTIBLE,
       LocationType::kHome,
       {1.0, 1.0, 1.0, 1.0, 1.0, 1.0, 1.0}},
      {{.retention_horizon = absl::InfiniteFuture(),
        .latest_contact_time = absl::InfinitePast()},
       HealthState::EXPOSED,
       LocationType::kHome,
       {1.0, 1.0, 1.0, 1.0, 1.0, 1.0, 1.0}},
      {{.retention_horizon = absl::FromUnixSeconds(0LL),
        .latest_contact_time = absl::FromUnixSeconds(86400LL)},
       HealthState::EXPOSED,
       LocationType::kHome,
       {1.0, 1.0, 1.0, 1.0, 1.0, 1.0, 1.0}},
      {{.retention_horizon = absl::InfiniteFuture(),
        .latest_contact_time = absl::InfinitePast()},
       HealthState::SUSCEPTIBLE,
       LocationType::kHome,
       {1.0, 1.0, 1.0, 1.0, 1.0, 1.0, 1.0}},
  };
  for (const Case& c : cases) {
    EXPECT_THAT(
        FrequencyAdjustments(policy_.get(), c.health_state, c.contact_summary,
                             c.location_type, test_days),
        testing::ElementsAreArray(c.expected_frequency_adjustments))
        << c;
  }
}

TEST_F(PublicPolicyTest, GetsTestPolicyUntested) {
  EXPECT_THAT(
      policy_->GetTestPolicy(
          {.retention_horizon = absl::FromUnixSeconds(0LL),
           .latest_contact_time = absl::FromUnixSeconds(1LL)},
          {.time_requested = absl::InfiniteFuture(),
           .time_received = absl::InfiniteFuture(),
           .needs_retry = false,
           .probability = 0.0f}),
      Eq(PublicPolicy::TestPolicy{.should_test = true,
                                  .time_requested = absl::FromUnixSeconds(1LL),
                                  .latency = absl::Hours(24)}));
}

TEST_F(PublicPolicyTest, GetsTestPolicyRetry) {
  EXPECT_THAT(
      policy_->GetTestPolicy(
          {.retention_horizon = absl::FromUnixSeconds(86400LL),
           .latest_contact_time = absl::FromUnixSeconds(90000LL)},
          {.time_requested = absl::FromUnixSeconds(1LL),
           .time_received = absl::InfiniteFuture(),
           .needs_retry = true,
           .probability = 0.0f}),
      Eq(PublicPolicy::TestPolicy{.should_test = true,
                                  .time_requested = absl::FromUnixSeconds(1LL),
                                  .latency = absl::Hours(24)}));
}

TEST_F(PublicPolicyTest, GetsTestPolicyTooSoonForRetest) {
  EXPECT_THAT(policy_->GetTestPolicy(
                  {.retention_horizon = absl::FromUnixSeconds(0LL),
                   .latest_contact_time = absl::FromUnixSeconds(1LL)},
                  {.time_requested = absl::FromUnixSeconds(1LL),
                   .time_received = absl::FromUnixSeconds(86401LL),
                   .needs_retry = false,
                   .probability = 0.0f}),
              Eq(PublicPolicy::TestPolicy{.should_test = false}));
}

TEST_F(PublicPolicyTest, GetsTestPolicyRetest) {
  EXPECT_THAT(policy_->GetTestPolicy(
                  {.retention_horizon = absl::FromUnixSeconds(1000000LL),
                   .latest_contact_time = absl::FromUnixSeconds(1001000LL)},
                  {.time_requested = absl::FromUnixSeconds(1LL),
                   .time_received = absl::FromUnixSeconds(86400LL),
                   .needs_retry = false,
                   .probability = 0.0f}),
              Eq(PublicPolicy::TestPolicy{
                  .should_test = true,
                  .time_requested = absl::FromUnixSeconds(1001000LL),
                  .latency = absl::Hours(24)}));
}

TEST_F(PublicPolicyTest, GetsTestPolicyNoRetestNeeded) {
  EXPECT_THAT(policy_->GetTestPolicy(
                  {.retention_horizon = absl::FromUnixSeconds(1000000LL),
                   .latest_contact_time = absl::FromUnixSeconds(1001000LL)},
                  {.time_requested = absl::FromUnixSeconds(1LL),
                   .time_received = absl::FromUnixSeconds(86400LL),
                   .needs_retry = false,
                   .probability = 1.0f}),
              Eq(PublicPolicy::TestPolicy{.should_test = false}));
}

TEST_F(PublicPolicyTest, GetsTestPolicyNoRecentContacts) {
  EXPECT_THAT(policy_->GetTestPolicy(
                  {.retention_horizon = absl::FromUnixSeconds(1086400LL),
                   .latest_contact_time = absl::FromUnixSeconds(0LL)},
                  {.time_requested = absl::FromUnixSeconds(0LL),
                   .time_received = absl::FromUnixSeconds(43200LL),
                   .needs_retry = false,
                   .probability = 0.0f}),
              Eq(PublicPolicy::TestPolicy{.should_test = false}));
}

TEST_F(PublicPolicyTest, GetsContactTracingPolicy) {
  EXPECT_THAT(policy_->GetContactTracingPolicy({}, {}),
              Eq(PublicPolicy::ContactTracingPolicy{
                  .report_recursively = false, .send_positive_test = true}));
}

TEST_F(PublicPolicyTest, GetsContactRetentionDuration) {
  EXPECT_EQ(policy_->ContactRetentionDuration(), absl::Hours(336));
}

}  // namespace
}  // namespace abesim
