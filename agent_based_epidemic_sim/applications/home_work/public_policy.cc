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

#include "agent_based_epidemic_sim/applications/home_work/public_policy.h"

#include <algorithm>

#include "absl/memory/memory.h"
#include "absl/random/distributions.h"
#include "absl/time/time.h"
#include "agent_based_epidemic_sim/applications/home_work/config.pb.h"
#include "agent_based_epidemic_sim/applications/home_work/location_type.h"
#include "agent_based_epidemic_sim/core/integral_types.h"
#include "agent_based_epidemic_sim/core/public_policy.h"
#include "agent_based_epidemic_sim/port/time_proto_util.h"

namespace abesim {

namespace {

// A policy that toggles between going to work and not going to work at
// defined times.
class TogglingPolicy : public PublicPolicy {
 public:
  TogglingPolicy(LocationTypeFn location_type, std::vector<absl::Time> toggles)
      : location_type_(std::move(location_type)),
        toggles_(std::move(toggles)) {}

  VisitAdjustment GetVisitAdjustment(const Timestep& timestep,
                                     HealthState::State health_state,
                                     const ContactSummary& contact_summary,
                                     const int64 location_uuid) const override {
    return {
        .frequency_adjustment =
            SkipVisit(timestep, location_uuid) ? 0.0f : 1.0f,
        .duration_adjustment = 1.0f,
    };
  }

  TestPolicy GetTestPolicy(
      const ContactSummary& contact_summary,
      const TestResult& previous_test_result) const override {
    return {.should_test = false,
            .time_requested = absl::InfiniteFuture(),
            .latency = absl::InfiniteDuration()};
  }

  ContactTracingPolicy GetContactTracingPolicy(
      absl::Span<const ContactReport> received_contact_reports,
      const TestResult& test_result) const override {
    return {.report_recursively = false, .send_positive_test = false};
  }

  absl::Duration ContactRetentionDuration() const override {
    return absl::ZeroDuration();
  }

 private:
  bool SkipVisit(const Timestep& timestep, const int64 location_uuid) const {
    if (location_type_(location_uuid) != LocationType::kWork) return false;
    auto iter =
        std::lower_bound(toggles_.begin(), toggles_.end(), timestep.end_time());
    if (iter == toggles_.begin()) {
      return false;
    }
    iter--;
    // If multiple toggles were active during a single timestep, it's not
    // obvious what to do. Arbitrarily we decide that in this case we just
    // go to work that timestep as usual.
    if (*iter > timestep.start_time()) {
      return false;
    }
    // The toggle array implicitly starts in a state where we go to work.  This
    // corresponds to the fact that DistancingPolicy protos always implicitly
    // have a stage that starts at infinite past with an
    // essential_worker_fraction of 1.0.  Therefore the 0th entry in the array
    // is the time we stop going to work, the 1st entry is when we start working
    // again, etc.
    return (iter - toggles_.begin()) % 2 == 0;
  }
  const LocationTypeFn location_type_;
  const std::vector<absl::Time> toggles_;
};

}  // namespace

const PublicPolicy* TogglePolicyGenerator::NextPolicy() {
  return GetPolicy(absl::Uniform(gen_, 0.0, 1.0));
}

const PublicPolicy* TogglePolicyGenerator::GetPolicy(
    const float essentialness) const {
  auto iter =
      std::lower_bound(tiers_.begin(), tiers_.end(), essentialness,
                       [](const Tier& tier, float essentialness) {
                         return tier.essential_worker_fraction < essentialness;
                       });
  if (iter == tiers_.begin()) {
    return noop_policy_.get();
  }
  iter--;
  return iter->policy.get();
}

TogglePolicyGenerator::TogglePolicyGenerator(std::vector<Tier> tiers)
    : noop_policy_(NewNoOpPolicy()), tiers_(std::move(tiers)) {}

StatusOr<std::unique_ptr<TogglePolicyGenerator>> NewPolicyGenerator(
    const DistancingPolicy& config, LocationTypeFn location_type) {
  struct DistancingStage {
    absl::Time start_time;
    float essential_worker_fraction;
  };
  std::vector<DistancingStage> stages;
  std::vector<TogglePolicyGenerator::Tier> tiers;
  for (const DistancingStageProto& stage : config.stages()) {
    auto start_time_or = DecodeGoogleApiProto(stage.start_time());
    if (!start_time_or.ok()) return start_time_or.status();
    stages.push_back({*start_time_or, stage.essential_worker_fraction()});
    tiers.push_back(
        {.essential_worker_fraction = stage.essential_worker_fraction()});
  }
  std::sort(stages.begin(), stages.end(),
            [](const DistancingStage& a, const DistancingStage& b) {
              return a.start_time < b.start_time;
            });
  std::sort(tiers.begin(), tiers.end(),
            [](const TogglePolicyGenerator::Tier& a,
               const TogglePolicyGenerator::Tier& b) {
              return a.essential_worker_fraction < b.essential_worker_fraction;
            });
  struct TierToggles {
    std::vector<absl::Time> toggles;
    bool working = true;
  };
  std::vector<TierToggles> tier_toggles(tiers.size());
  for (const DistancingStage& stage : stages) {
    for (int i = 0; i < tiers.size(); ++i) {
      TierToggles& state = tier_toggles[i];
      const TogglePolicyGenerator::Tier& tier = tiers[i];
      bool working =
          tier.essential_worker_fraction < stage.essential_worker_fraction;
      if (working != state.working) {
        state.working = working;
        state.toggles.push_back(stage.start_time);
      }
    }
  }
  for (int i = 0; i < tiers.size(); ++i) {
    tiers[i].policy = absl::make_unique<TogglingPolicy>(
        location_type, std::move(tier_toggles[i].toggles));
  }
  return absl::WrapUnique(new TogglePolicyGenerator(std::move(tiers)));
}

}  // namespace abesim
