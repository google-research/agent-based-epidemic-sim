#ifndef AGENT_BASED_EPIDEMIC_SIM_UTIL_TEST_UTIL_H_
#define AGENT_BASED_EPIDEMIC_SIM_UTIL_TEST_UTIL_H_

#include "absl/time/time.h"
#include "absl/types/optional.h"
#include "agent_based_epidemic_sim/core/agent.h"
#include "agent_based_epidemic_sim/core/broker.h"
#include "agent_based_epidemic_sim/core/event.h"
#include "agent_based_epidemic_sim/core/integral_types.h"
#include "agent_based_epidemic_sim/core/location.h"
#include "agent_based_epidemic_sim/core/risk_score.h"
#include "agent_based_epidemic_sim/core/risk_score_model.h"
#include "agent_based_epidemic_sim/core/timestep.h"
#include "agent_based_epidemic_sim/core/transition_model.h"
#include "agent_based_epidemic_sim/core/transmission_model.h"
#include "agent_based_epidemic_sim/core/visit.h"
#include "agent_based_epidemic_sim/core/visit_generator.h"
#include "gmock/gmock.h"

namespace abesim {

std::vector<const Exposure*> MakePointers(const std::vector<Exposure>& v) {
  std::vector<const Exposure*> result;
  result.reserve(v.size());
  for (const Exposure& e : v) {
    result.push_back(&e);
  }
  return result;
}

class MockTransitionModel : public TransitionModel {
 public:
  explicit MockTransitionModel() = default;
  MOCK_METHOD(HealthTransition, GetNextHealthTransition,
              (const HealthTransition& latest_transition), (override));
};

class MockTransmissionModel : public TransmissionModel {
 public:
  MockTransmissionModel() = default;
  MOCK_METHOD(HealthTransition, GetInfectionOutcome,
              (absl::Span<const Exposure* const> exposures), (override));
};

class MockVisitGenerator : public VisitGenerator {
 public:
  explicit MockVisitGenerator() = default;
  MOCK_METHOD(void, GenerateVisits,
              (const Timestep& timestep, const RiskScore& policy,
               std::vector<Visit>* visits),
              (const, override));
};

template <typename T>
class MockBroker : public Broker<T> {
 public:
  explicit MockBroker() = default;
  MOCK_METHOD(void, Send, (absl::Span<const T> visits), (override));
};

class MockRiskScore : public RiskScore {
 public:
  MOCK_METHOD(void, AddHealthStateTransistion, (HealthTransition transition),
              (override));
  MOCK_METHOD(void, UpdateLatestTimestep, (const Timestep& timestep),
              (override));
  MOCK_METHOD(void, AddExposureNotification,
              (const Exposure& contact, const ContactReport& notification),
              (override));
  MOCK_METHOD(VisitAdjustment, GetVisitAdjustment,
              (const Timestep& timestep, int64 location_uuid),
              (const, override));
  MOCK_METHOD(TestResult, GetTestResult, (const Timestep& timestep),
              (const, override));
  MOCK_METHOD(ContactTracingPolicy, GetContactTracingPolicy,
              (const Timestep& timestep), (const, override));
  MOCK_METHOD(absl::Duration, ContactRetentionDuration, (), (const, override));
  MOCK_METHOD(float, GetRiskScore, (), (const, override));
  MOCK_METHOD(void, RequestTest, (absl::Time time), (override));
};

class MockRiskScoreModel : public RiskScoreModel {
 public:
  MOCK_METHOD(float, ComputeRiskScore,
              (const Exposure& exposure,
               absl::optional<absl::Time> initial_symtom_onset_time),
              (const));
};

class MockAgent : public Agent {
 public:
  MOCK_METHOD(int64, uuid, (), (const, override));
  MOCK_METHOD(void, ComputeVisits,
              (const Timestep& timestep, Broker<Visit>* visit_broker),
              (const, override));
  MOCK_METHOD(void, ProcessInfectionOutcomes,
              (const Timestep& timestep,
               absl::Span<const InfectionOutcome> infection_outcomes),
              (override));
  MOCK_METHOD(void, UpdateContactReports,
              (const Timestep& timestep,
               absl::Span<const ContactReport> symptom_reports,
               Broker<ContactReport>* symptom_broker),
              (override));
  MOCK_METHOD(HealthState::State, CurrentHealthState, (), (const, override));
  MOCK_METHOD(TestResult, CurrentTestResult, (const Timestep&),
              (const, override));
  MOCK_METHOD(absl::Span<const HealthTransition>, HealthTransitions, (),
              (const, override));
  MOCK_METHOD(std::optional<absl::Time>, symptom_onset, (), (const, override));
  MOCK_METHOD(std::optional<absl::Time>, infection_onset, (),
              (const, override));
  MOCK_METHOD(const ExposureStore*, exposure_store, (), (const, override));
};

class MockLocation : public Location {
 public:
  MOCK_METHOD(int64, uuid, (), (const, override));

  // Process a set of visits and write InfectionOutcomes to the given
  // infection_broker.  If observer != nullptr, then observer should be called
  // for each visit with a list of corresponding contacts.
  MOCK_METHOD(void, ProcessVisits,
              (absl::Span<const Visit> visits,
               Broker<InfectionOutcome>* infection_broker),
              (override));
};

}  // namespace abesim

#endif  // AGENT_BASED_EPIDEMIC_SIM_UTIL_TEST_UTIL_H_
