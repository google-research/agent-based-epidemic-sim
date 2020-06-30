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

#ifndef THIRD_PARTY_AGENT_BASED_EPIDEMIC_SIM_APPLICATIONS_HOME_WORK_PUBLIC_POLICY_H_
#define THIRD_PARTY_AGENT_BASED_EPIDEMIC_SIM_APPLICATIONS_HOME_WORK_PUBLIC_POLICY_H_

#include "absl/random/random.h"
#include "agent_based_epidemic_sim/applications/home_work/config.pb.h"
#include "agent_based_epidemic_sim/applications/home_work/location_type.h"
#include "agent_based_epidemic_sim/core/public_policy.h"
#include "agent_based_epidemic_sim/port/statusor.h"

namespace abesim {

class TogglePolicyGenerator : public PolicyGenerator {
 public:
  const PublicPolicy* NextPolicy();
  // Get a policy for a worker with a given 'essentialness'.  Essentialness
  // measures the fraction of the population more essential than the given
  // worker, so a score of .2 means 20% of workers are more essential, and 80%
  // are less essential.  A worker with essentialness E will work only if the
  // current poublic policy has an essential_worker_fraction >= E.
  const PublicPolicy* GetPolicy(float essentialness) const;

 private:
  friend StatusOr<std::unique_ptr<TogglePolicyGenerator>> NewPolicyGenerator(
      const DistancingPolicy& config, LocationTypeFn location_type);

  // For every value of essential_worker_fraction in the input policy we keep
  // a Tier for workers who fall into the essentialness band between that
  // fraction and the next higher essential_worker_fraction.
  struct Tier {
    std::unique_ptr<PublicPolicy> policy;
    float essential_worker_fraction;
  };

  explicit TogglePolicyGenerator(std::vector<Tier> tiers);

  absl::BitGen gen_;
  const std::unique_ptr<const PublicPolicy> noop_policy_;
  const std::vector<Tier> tiers_;
};

StatusOr<std::unique_ptr<TogglePolicyGenerator>> NewPolicyGenerator(
    const DistancingPolicy& config, LocationTypeFn location_type);

}  // namespace abesim

#endif  // THIRD_PARTY_AGENT_BASED_EPIDEMIC_SIM_APPLICATIONS_HOME_WORK_PUBLIC_POLICY_H_
