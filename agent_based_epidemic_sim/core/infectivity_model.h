#ifndef AGENT_BASED_EPIDEMIC_SIM_CORE_INFECTIVITY_MODEL_H_
#define AGENT_BASED_EPIDEMIC_SIM_CORE_INFECTIVITY_MODEL_H_

#include "absl/time/time.h"
#include "agent_based_epidemic_sim/core/pandemic.pb.h"

namespace abesim {

// Models infectivity as a function of symptoms.
class InfectivityModel {
 public:
  // Symptom-dependent factor.
  virtual float SymptomFactor(HealthState::State health_state) const = 0;

  virtual float Infectivity(absl::Duration duration_since_infection) const = 0;

  // TODO: Move susceptibility computation here.
  // virtual float Susceptibility(...) const;

  virtual ~InfectivityModel() = default;
};

}  // namespace abesim

#endif  // AGENT_BASED_EPIDEMIC_SIM_CORE_INFECTIVITY_MODEL_H_
