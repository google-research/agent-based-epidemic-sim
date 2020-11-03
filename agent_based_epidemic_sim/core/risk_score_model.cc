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

#include "agent_based_epidemic_sim/core/risk_score_model.h"

#include "agent_based_epidemic_sim/core/event.h"

namespace abesim {
namespace {

class NullRiskScoreModel : public RiskScoreModel {
 public:
  float ComputeRiskScore(
      const Exposure& exposure,
      absl::optional<absl::Time> initial_symtom_onset_time) const override {
    return 0;
  }
};

}  // namespace

std::unique_ptr<RiskScoreModel> NewNullRiskScoreModel() {
  return absl::make_unique<NullRiskScoreModel>();
}

}  // namespace abesim
