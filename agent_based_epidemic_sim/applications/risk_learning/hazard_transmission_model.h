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

#ifndef AGENT_BASED_EPIDEMIC_SIM_APPLICATIONS_RISK_LEARNING_HAZARD_TRANSMISSION_MODEL_H_
#define AGENT_BASED_EPIDEMIC_SIM_APPLICATIONS_RISK_LEARNING_HAZARD_TRANSMISSION_MODEL_H_

#include <cmath>
#include <memory>
#include <type_traits>

#include "absl/memory/memory.h"
#include "agent_based_epidemic_sim/core/constants.h"
#include "agent_based_epidemic_sim/core/event.h"
#include "agent_based_epidemic_sim/core/timestep.h"
#include "agent_based_epidemic_sim/core/transmission_model.h"

namespace abesim {

struct HazardTransmissionOptions {
  float lambda = 1;

  // TODO: Fix variable naming. For now easiest to just mirror
  // (broken link) until stable.
  std::function<float(float)> risk_at_distance_function = [](float proximity) {
    const float a = 1.5;
    const float b = 6.6;
    return 1 - 1 / (1 + std::exp(-a * proximity + b));
  };
};

// Models transmission between hosts as a sum of doses where each dose computes
// a hazard as a function of (duration, distance, infectivity, symptom_factor,
// location_transmissibility, susceptibility).

// TODO: Summarize the transmission model more concisely when it is
// stable.

// TODO: Currently this TransmissionModel should only be used in
// conjunction with Graph simulator. Extend support to Location event simulator.
class HazardTransmissionModel : public TransmissionModel {
 public:
  HazardTransmissionModel(
      HazardTransmissionOptions options = HazardTransmissionOptions(),
      std::function<void(const float, const absl::Time)> hazard_callback =
          [](const float, const absl::Time) { return; })
      : lambda_(options.lambda),
        hazard_callback_(std::move(hazard_callback)),
        risk_at_distance_function_(
            std::move(options.risk_at_distance_function)) {}

  // Computes the infection outcome given exposures.
  HealthTransition GetInfectionOutcome(
      absl::Span<const Exposure* const> exposures) override;

  // Computes a "viral dose" which is used directly in computing the probability
  // of infection for a given Exposure.
  float ComputeDose(float distance, absl::Duration duration,
                    const Exposure* exposure) const;

 private:
  // TODO: Link out to actual papers or some other authoritative
  // source.
  // Typical values of lambda_ come from:
  //  Amanda Wilson paper
  //  (https://www.medrxiv.org/content/10.1101/2020.07.17.20156539v1): 2.2x10e-3
  //  Mark Briers paper (https://arxiv.org/abs/2005.11057): 0.6 / 15
  float lambda_;
  std::function<void(float, absl::Time)> hazard_callback_;
  // Generates a risk dosage for a given distance.
  std::function<float(float)> risk_at_distance_function_;
};

class Hazard {
 public:
  explicit Hazard(
      HazardTransmissionOptions options = HazardTransmissionOptions()) {
    auto hazard_callback = [this](const float prob_infection,
                                  const absl::Time time) {
      hazard_ = prob_infection;
      time_ = time;
    };
    transmission_model_ = absl::make_unique<HazardTransmissionModel>(
        options, std::move(hazard_callback));
  }

  TransmissionModel* GetTransmissionModel() {
    return transmission_model_.get();
  }

  float GetHazard(const Timestep& timestep) const {
    if (timestep.start_time() - timestep.duration() > time_) {
      // Hazard is stale.
      return 0.0f;
    }
    return hazard_;
  }

 private:
  float hazard_ = 0;
  absl::Time time_ = absl::InfinitePast();
  std::unique_ptr<TransmissionModel> transmission_model_;
};

}  // namespace abesim

#endif  // AGENT_BASED_EPIDEMIC_SIM_APPLICATIONS_RISK_LEARNING_HAZARD_TRANSMISSION_MODEL_H_
