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

#include "absl/status/statusor.h"
#include "absl/time/time.h"
#include "absl/types/optional.h"
#include "agent_based_epidemic_sim/applications/risk_learning/config.pb.h"
#include "agent_based_epidemic_sim/core/location_type.h"
#include "agent_based_epidemic_sim/core/risk_score.h"

namespace abesim {

struct LearningRiskScorePolicy {
  // The number of days of exposure history to use when determining whether to
  // take policy actions e.g. quarantine, test.
  // Defaults to 14 days as this is standard in the literature.
  int exposure_notification_window_days_ = 14;

  // Overall scaling factor for risk score. This scales the product of duration
  // and infection scores.
  // Default value from "Quantifying SARS-CoV-2-infection risk withing the
  // Apple/Google exposure notification framework to inform quarantine
  // recommendations, Amanda Wilson, Nathan Aviles, Paloma Beamer,
  // Zsombor Szabo, Kacey Ernst, Joanna Masel. July 2020."
  // https://www.medrxiv.org/content/10.1101/2020.07.17.20156539v2
  float risk_scale_factor_ = 3.1 * 1e-4;
};

// A static representation of the risk score model loaded at simulation start.
// All usages of this class should be const& to prevent us creating a new copy
// for each agent.
class LearningRiskScoreModel {
 public:
  LearningRiskScoreModel() {}
  LearningRiskScoreModel(
      const std::vector<BLEBucket>& ble_buckets,
      const std::vector<InfectiousnessBucket>& infectiousness_buckets)
      : ble_buckets_(ble_buckets),
        infectiousness_buckets_(infectiousness_buckets) {}

  float ComputeRiskScore(
      const Exposure& exposure,
      absl::optional<absl::Time> initial_symptom_onset_time) const;

 private:
  absl::StatusOr<int> AttenuationToBinIndex(const int attenuation) const;

  float ComputeDurationRiskScore(const Exposure& exposure) const;
  // Note: This method assumes infectiousness_buckets_ has a particular
  // ordering. Specifically the ordering is asc on days_since_symptom_onset_max.
  float ComputeInfectionRiskScore(
      absl::optional<int64> days_since_symptom_onset) const;

  // Buckets representing threshold and corresponding weight of ble attenuation
  // signals.
  std::vector<BLEBucket> ble_buckets_;
  // Buckets representing days_since_symptom onset and a mapping to a
  // corresponding infectiousness level and model weight.
  std::vector<InfectiousnessBucket> infectiousness_buckets_;
};

absl::StatusOr<const LearningRiskScoreModel> CreateLearningRiskScoreModel(
    const LearningRiskScoreModelProto& proto);

absl::StatusOr<const LearningRiskScorePolicy> CreateLearningRiskScorePolicy(
    const LearningRiskScorePolicyProto& proto);

absl::StatusOr<std::unique_ptr<RiskScore>> CreateLearningRiskScore(
    const TracingPolicyProto& proto,
    const LearningRiskScoreModel& risk_score_model,
    const LearningRiskScorePolicy& risk_score_policy,
    LocationTypeFn location_type);

// Returns a risk score that toggles contact tracing behavior on the basis of
// whether it is enabled.
std::unique_ptr<RiskScore> CreateAppEnabledRiskScore(
    bool is_app_enabled, std::unique_ptr<RiskScore> risk_score);

}  // namespace abesim

#endif  // AGENT_BASED_EPIDEMIC_SIM_APPLICATIONS_RISK_LEARNING_RISK_SCORE_H_
