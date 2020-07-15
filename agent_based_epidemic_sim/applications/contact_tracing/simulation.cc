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

#include "agent_based_epidemic_sim/applications/contact_tracing/simulation.h"

#include "absl/strings/string_view.h"
#include "agent_based_epidemic_sim/applications/contact_tracing/public_policy.h"
#include "agent_based_epidemic_sim/applications/home_work/location_type.h"
#include "agent_based_epidemic_sim/applications/home_work/simulation.h"
#include "agent_based_epidemic_sim/core/public_policy.h"

namespace abesim {
namespace {

class TracingPolicyGenerator : public PolicyGenerator {
 public:
  explicit TracingPolicyGenerator(std::unique_ptr<PublicPolicy> policy)
      : policy_(std::move(policy)) {}

  const PublicPolicy* NextPolicy() override { return policy_.get(); }

 private:
  std::unique_ptr<PublicPolicy> policy_;
};

}  // namespace

void RunSimulation(absl::string_view output_file_path,
                   absl::string_view learning_output_base,
                   const ContactTracingHomeWorkSimulationConfig& config,
                   int num_workers) {
  auto get_policy_generator = [&config](LocationTypeFn location_type) {
    return absl::make_unique<TracingPolicyGenerator>(
        *CreateTracingPolicy(config.tracing_policy(), location_type));
  };
  auto context = GetSimulationContext(config.home_work_config());
  RunSimulation(output_file_path, learning_output_base,
                config.home_work_config(), get_policy_generator, num_workers,
                context);
}

}  // namespace abesim
