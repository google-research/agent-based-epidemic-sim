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

// TODO: Rename this file to infection_outcome.h or similar.
#ifndef AGENT_BASED_EPIDEMIC_SIM_CORE_EVENT_H_
#define AGENT_BASED_EPIDEMIC_SIM_CORE_EVENT_H_

#include "absl/meta/type_traits.h"
#include "absl/time/time.h"
#include "absl/types/variant.h"
#include "agent_based_epidemic_sim/core/integral_types.h"
#include "agent_based_epidemic_sim/core/visit.h"
#include "agent_based_epidemic_sim/util/ostream_overload.h"

namespace abesim {

inline constexpr uint8 kNumberMicroExposureBuckets = 10;

// Convenience method to easily output contents of array to ostream rather than
// constructing output string manually.
template <typename T, size_t N>
std::ostream& operator<<(std::ostream& os, const std::array<T, N>& a) {
  os << "[";
  for (int i = 0; i < a.size(); i++) {
    os << a[i];
    if (i != a.size() - 1) os << ", ";
  }
  os << "]";
  return os;
}

// An event representing a HealthState transition.
struct HealthTransition {
  absl::Time time;
  HealthState::State health_state;

  friend bool operator==(const HealthTransition& a, const HealthTransition& b) {
    return (a.time == b.time && a.health_state == b.health_state);
  }

  friend bool operator!=(const HealthTransition& a, const HealthTransition& b) {
    return !(a == b);
  }

  friend std::ostream& operator<<(std::ostream& strm,
                                  const HealthTransition& health_transition) {
    return strm << "{" << health_transition.time << ", "
                << health_transition.health_state << "}";
  }
};

static_assert(absl::is_trivially_copy_constructible<HealthTransition>::value,
              "HealthTransition must be trivially copyable.");

// Represents a single exposure event between a SUSCEPTIBLE and an INFECTIOUS
// entity made up of multiple "micro exposure" events.
// Each exposure has real values that are used when computing the probability of
// transmission from the INFECTIOUS entity to the SUSCEPTIBLE entity:
//   infectivity: A function of the duration since the INFECTIOUS entity first
//   became infected.
//   symptom_factor: A function of the current HealthState of the INFECTIOUS
//   entity.
// Each count in micro_exposure_counts represents a duration in minutes that the
// two entities spent at a particular distance (as represented by the index).
struct Exposure {
  absl::Time start_time;
  absl::Duration duration;
  std::array<uint8, kNumberMicroExposureBuckets> micro_exposure_counts = {};
  float infectivity;
  float symptom_factor;

  friend bool operator==(const Exposure& a, const Exposure& b) {
    return (a.start_time == b.start_time && a.duration == b.duration &&
            a.infectivity == b.infectivity &&
            a.micro_exposure_counts == b.micro_exposure_counts);
  }

  friend bool operator!=(const Exposure& a, const Exposure& b) {
    return !(a == b);
  }

  friend std::ostream& operator<<(std::ostream& strm,
                                  const Exposure& exposure) {
    return strm << "{" << exposure.start_time << ", " << exposure.duration
                << ", " << exposure.infectivity << ", "
                << exposure.micro_exposure_counts << "}";
  }
};

static_assert(absl::is_trivially_copy_constructible<Exposure>::value,
              "Exposure must be trivially copyable.");

// Represents a persons contact with another person.
// TODO: Remove HealthState::State and represent solely via
// infectivity.
struct Contact {
  int64 other_uuid;
  HealthState::State other_state;
  Exposure exposure;

  friend bool operator==(const Contact& a, const Contact& b) {
    return (a.other_uuid == b.other_uuid && a.other_state == b.other_state &&
            a.exposure == b.exposure);
  }

  friend bool operator!=(const Contact& a, const Contact& b) {
    return !(a == b);
  }

  friend std::ostream& operator<<(std::ostream& strm, const Contact& contact) {
    return strm << "{" << contact.other_uuid << ", " << contact.other_state
                << ", " << contact.exposure << "}";
  }
};

static_assert(absl::is_trivially_copy_constructible<Contact>::value,
              "Contact must be trivially copyable.");

// Represents the earliest and latest times of retained positive test result
// contacts.
// Note: finite values for the times imply that they are active in terms of
// the agent needing to take them into account in determining its actions.
// TODO: Widen interface to accept self-reported symptoms, and the viral
// load/infectivity of contacts.
struct ContactSummary {
  absl::Time retention_horizon;
  absl::Time latest_contact_time;

  friend bool operator==(const ContactSummary& a, const ContactSummary& b) {
    return (a.retention_horizon == b.retention_horizon &&
            a.latest_contact_time == b.latest_contact_time);
  }

  friend bool operator!=(const ContactSummary& a, const ContactSummary& b) {
    return !(a == b);
  }

  friend std::ostream& operator<<(std::ostream& strm,
                                  const ContactSummary& contact_summary) {
    return strm << "{" << contact_summary.retention_horizon << ", "
                << contact_summary.latest_contact_time << "}";
  }
};

static_assert(absl::is_trivially_copy_constructible<ContactSummary>::value,
              "ContactSummary must be trivially copyable.");

// An outcome of a visit to a location.
// May contain a contact, or background_exposure, representing an exposure
// to an INFECTIOUS entity (contact, fomite, location etc.) with a given
// infectivity.
// TODO: Rename to VisitOutcome. Also, consider a move to v<Contact>.
struct InfectionOutcome {
  int64 agent_uuid;
  Exposure exposure;
  InfectionOutcomeProto::ExposureType exposure_type;
  int64 source_uuid;

  friend bool operator==(const InfectionOutcome& a, const InfectionOutcome& b) {
    return (a.agent_uuid == b.agent_uuid && a.exposure == b.exposure &&
            a.exposure_type == b.exposure_type &&
            a.source_uuid == b.source_uuid);
  }

  friend bool operator!=(const InfectionOutcome& a, const InfectionOutcome& b) {
    return !(a == b);
  }

  friend std::ostream& operator<<(std::ostream& strm,
                                  const InfectionOutcome& infection_outcome) {
    return strm << "{" << infection_outcome.agent_uuid << ", "
                << infection_outcome.exposure << ", "
                << infection_outcome.exposure_type << ", "
                << infection_outcome.source_uuid << "}";
  }
};

static_assert(absl::is_trivially_copy_constructible<InfectionOutcome>::value,
              "InfectionOutcome must be trivially copyable.");

// The outcome of a test.
// TODO: Refine how to report a test outcome and confidence.
// Currently, it is represented by a float in [0, 1].
// Note: if a test result has needs_retry set, it was ordered too far in
// the future and should be redone. This could happen in the following scenario:
// Agent A requests a test at time t_0 and receives the result at t_1.
// Agent B receives the (positive) TestResult of agent A in a ContactReport.
// Agent B requests a test at t_1. However, agent B does not have a determined
// next state after t_1, and so it cannot determine the result of the test. It
// must wait until it has a call to ProcessInfectionOutcomes that produces a
// state transition beyond t_1 so that it knows its state definitively at t_1.
// Note that this does not violate causality even though it is future-looking
// as agents only act on future state transitions during calls to
// ProcessInfectionOutcomes and ComputeVisits, which contain information about
// the current timestep.
struct TestResult {
  absl::Time time_requested;
  absl::Time time_received;
  float probability;

  friend bool operator==(const TestResult& a, const TestResult& b) {
    return (a.time_requested == b.time_requested &&
            a.time_received == b.time_received &&
            a.probability == b.probability);
  }

  friend bool operator!=(const TestResult& a, const TestResult& b) {
    return !(a == b);
  }

  friend std::ostream& operator<<(std::ostream& strm,
                                  const TestResult& test_result) {
    return strm << "{" << test_result.time_requested << ", "
                << test_result.time_received << ", " << test_result.probability
                << "}";
  }
};

static_assert(absl::is_trivially_copy_constructible<TestResult>::value,
              "TestResult must be trivially copyable.");

// Health information that is to be broadcasted to a contact.
// TODO: Represent and report self-reported symptoms.
struct ContactReport {
  int64 from_agent_uuid;
  int64 to_agent_uuid;
  TestResult test_result;

  friend bool operator==(const ContactReport& a, const ContactReport& b) {
    return (a.from_agent_uuid == b.from_agent_uuid &&
            a.to_agent_uuid == b.to_agent_uuid &&
            a.test_result == b.test_result);
  }

  friend bool operator!=(const ContactReport& a, const ContactReport& b) {
    return !(a == b);
  }

  friend std::ostream& operator<<(std::ostream& strm,
                                  const ContactReport& contact_report) {
    return strm << "{" << contact_report.from_agent_uuid << ", "
                << contact_report.to_agent_uuid << ", "
                << contact_report.test_result << "}";
  }
};

static_assert(absl::is_trivially_copy_constructible<ContactReport>::value,
              "ContactReport must be trivially copyable.");

}  // namespace abesim

#endif  // AGENT_BASED_EPIDEMIC_SIM_CORE_EVENT_H_
