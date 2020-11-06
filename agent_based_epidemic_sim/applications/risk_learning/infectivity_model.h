#ifndef AGENT_BASED_EPIDEMIC_SIM_APPLICATIONS_RISK_LEARNING_INFECTIVITY_MODEL_H_
#define AGENT_BASED_EPIDEMIC_SIM_APPLICATIONS_RISK_LEARNING_INFECTIVITY_MODEL_H_

#include "absl/time/time.h"
#include "agent_based_epidemic_sim/agent_synthesis/population_profile.pb.h"
#include "agent_based_epidemic_sim/core/infectivity_model.h"
#include "agent_based_epidemic_sim/core/pandemic.pb.h"

namespace abesim {

// Models infectivity as a function of symptoms.
class RiskLearningInfectivityModel : public InfectivityModel {
 public:
  RiskLearningInfectivityModel(const GlobalProfile& profile);

  float SymptomFactor(HealthState::State health_state) const override;

  float Infectivity(
      const absl::Duration duration_since_infection) const override;

 private:
  float asymptomatic_infectious_factor_;
  float mild_infectious_factor_;
};

}  // namespace abesim

#endif  // AGENT_BASED_EPIDEMIC_SIM_APPLICATIONS_RISK_LEARNING_INFECTIVITY_MODEL_H_
