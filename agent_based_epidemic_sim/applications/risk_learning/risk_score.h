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

#ifndef AGENT_BASED_EPIDEMIC_SIM_APPLICATIONS_RISK_LEARNING_RISK_SCORE_H_
#define AGENT_BASED_EPIDEMIC_SIM_APPLICATIONS_RISK_LEARNING_RISK_SCORE_H_

#include <memory>

#include "absl/status/statusor.h"
#include "absl/time/time.h"
#include "absl/types/optional.h"
#include "agent_based_epidemic_sim/applications/risk_learning/config.pb.h"
#include "agent_based_epidemic_sim/applications/risk_learning/hazard_transmission_model.h"
#include "agent_based_epidemic_sim/core/location_type.h"
#include "agent_based_epidemic_sim/core/risk_score.h"
#include "agent_based_epidemic_sim/core/risk_score_model.h"

namespace abesim {

// TODO: The following struct and two factory methods can be hidden
// within the implementation of a RiskScoreBuilder class.
struct LearningRiskScorePolicy {
  // The number of days of exposure history to use when determining whether to
  // take policy actions e.g. quarantine, test.
  // Defaults to 14 days as this is standard in the literature.
  int exposure_notification_window_days = 14;

  // Overall scaling factor for risk score. This scales the product of duration
  // and infection scores.
  // Default value from "Quantifying SARS-CoV-2-infection risk withing the
  // Apple/Google exposure notification framework to inform quarantine
  // recommendations, Amanda Wilson, Nathan Aviles, Paloma Beamer,
  // Zsombor Szabo, Kacey Ernst, Joanna Masel. July 2020."
  // https://www.medrxiv.org/content/10.1101/2020.07.17.20156539v2
  float risk_scale_factor = 3.1 * 1e-4;
};

absl::StatusOr<std::unique_ptr<RiskScoreModel>> CreateLearningRiskScoreModel(
    const LearningRiskScoreModelProto& proto);

std::unique_ptr<RiskScoreModel> CreateTimeVaryingRiskScoreModel(
    std::function<const RiskScoreModel*()> get_model_fn);

absl::StatusOr<const LearningRiskScorePolicy> CreateLearningRiskScorePolicy(
    const LearningRiskScorePolicyProto& proto);

absl::StatusOr<std::unique_ptr<RiskScore>> CreateLearningRiskScore(
    const TracingPolicyProto& tracing_policy_proto,
    const LearningRiskScorePolicy& risk_score_policy,
    const RiskScoreModel* risk_score_model, LocationTypeFn location_type);

// Returns a risk score that toggles contact tracing behavior on the basis of
// whether it is enabled.
std::unique_ptr<RiskScore> CreateAppEnabledRiskScore(
    bool is_app_enabled, std::unique_ptr<RiskScore> risk_score);

std::unique_ptr<RiskScore> CreateHazardQueryingRiskScore(
    std::unique_ptr<Hazard> hazard, std::unique_ptr<RiskScore> risk_score);

}  // namespace abesim

#endif  // AGENT_BASED_EPIDEMIC_SIM_APPLICATIONS_RISK_LEARNING_RISK_SCORE_H_
