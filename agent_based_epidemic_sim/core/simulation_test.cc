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

#include "agent_based_epidemic_sim/core/simulation.h"

#include "absl/container/flat_hash_map.h"
#include "absl/synchronization/mutex.h"
#include "absl/time/time.h"
#include "absl/types/span.h"
#include "agent_based_epidemic_sim/core/agent.h"
#include "agent_based_epidemic_sim/core/event.h"
#include "agent_based_epidemic_sim/core/location.h"
#include "agent_based_epidemic_sim/core/observer.h"
#include "agent_based_epidemic_sim/core/timestep.h"
#include "gtest/gtest.h"

namespace abesim {
namespace {
using OutcomeMap = absl::flat_hash_map<int, int>;
using VisitMap = absl::flat_hash_map<std::pair<int, int>, int>;
using ReportMap = absl::flat_hash_map<std::pair<int, int>, int>;
ABSL_CONST_INIT absl::Mutex map_mu(absl::kConstInit);

const int kNumLocations = 1024;
const int kNumAgents = 1024;
const int kVisitsPerAgent = 7;
const int kReportsPerAgent = 3;
const int kNumSteps = 5;

std::array<int, kVisitsPerAgent> VisitLocations(int64 agent_id) {
  std::array<int, kVisitsPerAgent> ret;
  for (int i = 0; i < kVisitsPerAgent; i++) {
    ret[i] = (agent_id + 7 * i) % kNumLocations;
  }
  return ret;
}

std::array<int, kReportsPerAgent> ReportRecipients(int64 agent_id) {
  std::array<int, kReportsPerAgent> ret;
  for (int i = 0; i < kReportsPerAgent; i++) {
    ret[i] = (agent_id + 5 * i) % kNumAgents;
  }
  return ret;
}

class FakeAgent : public Agent {
 public:
  FakeAgent(int64 uuid, OutcomeMap* outcome_counts, ReportMap* report_counts)
      : uuid_(uuid),
        outcome_counts_(outcome_counts),
        report_counts_(report_counts) {}
  int64 uuid() const override { return uuid_; }
  void ComputeVisits(const Timestep& timestep,
                     Broker<Visit>* visit_broker) const override {
    for (const int location_uuid : VisitLocations(uuid_)) {
      visit_broker->Send(
          {{.location_uuid = location_uuid, .agent_uuid = uuid_}});
    }
  }
  void ProcessInfectionOutcomes(
      const Timestep& timestep,
      absl::Span<const InfectionOutcome> infection_outcomes) override {
    for (const InfectionOutcome& outcome : infection_outcomes) {
      ASSERT_EQ(outcome.agent_uuid, uuid_);
    }
    {
      absl::MutexLock l(&map_mu);
      (*outcome_counts_)[uuid_] += infection_outcomes.size();
    }
    if (last_timestep_ == nullptr) {
      last_timestep_ = absl::make_unique<Timestep>(timestep);
    } else {
      ASSERT_EQ(timestep.start_time(), last_timestep_->end_time());
      ASSERT_EQ(timestep.duration(), absl::Hours(24));
      *last_timestep_ = timestep;
    }
  }
  void UpdateContactReports(const Timestep& timestep,
                            absl::Span<const ContactReport> symptom_reports,
                            Broker<ContactReport>* symptom_broker) override {
    {
      absl::MutexLock l(&map_mu);
      for (const auto& report : symptom_reports) {
        ASSERT_EQ(report.to_agent_uuid, uuid_);
        ASSERT_EQ(report.test_result.time_received,
                  last_timestep_->start_time());
        (*report_counts_)[{report.from_agent_uuid, report.to_agent_uuid}]++;
      }
    }
    for (const int to_agent_uuid : ReportRecipients(uuid_)) {
      symptom_broker->Send({{.from_agent_uuid = uuid_,
                             .to_agent_uuid = to_agent_uuid,
                             .test_result = {
                                 .time_received = last_timestep_->end_time(),
                             }}});
    }
  }

  HealthState::State CurrentHealthState() const override {
    return HealthState::SUSCEPTIBLE;
  }

  TestResult CurrentTestResult(const Timestep&) const override {
    return TestResult{};
  }

  absl::Span<const HealthTransition> HealthTransitions() const override {
    return {};
  }

 private:
  std::unique_ptr<Timestep> last_timestep_;
  int64 uuid_;
  OutcomeMap* outcome_counts_;
  ReportMap* report_counts_;
};

class FakeLocation : public Location {
 public:
  FakeLocation(int64 uuid, VisitMap* visit_counts)
      : uuid_(uuid), visit_counts_(visit_counts) {}

  int64 uuid() const override { return uuid_; }
  void ProcessVisits(absl::Span<const Visit> visits,
                     Broker<InfectionOutcome>* infection_broker) override {
    for (const Visit& visit : visits) {
      ASSERT_EQ(visit.location_uuid, uuid_);
      {
        absl::MutexLock l(&map_mu);
        (*visit_counts_)[{uuid_, visit.agent_uuid}]++;
      }
      infection_broker->Send({{.agent_uuid = visit.agent_uuid}});
    }
  }

 private:
  int64 uuid_;
  VisitMap* visit_counts_;
};

struct PerAgent {
  int observations = 0;
  int outcomes = 0;
};
struct PerLocation {
  int observations = 0;
  int visits = 0;
};

class FakeObserver : public AgentInfectionObserver,
                     public LocationVisitObserver {
 public:
  void Observe(const Agent& agent,
               absl::Span<const InfectionOutcome> outcomes) override {
    agent_stats_[agent.uuid()].observations++;
    agent_stats_[agent.uuid()].outcomes += outcomes.size();
  }
  void Observe(const Location& location,
               absl::Span<const Visit> visits) override {
    location_stats_[location.uuid()].observations++;
    location_stats_[location.uuid()].visits += visits.size();
  }

 private:
  friend class FakeObserverFactory;

  absl::flat_hash_map<int64, PerAgent> agent_stats_;
  absl::flat_hash_map<int64, PerLocation> location_stats_;
};

class FakeObserverFactory : public ObserverFactory<FakeObserver> {
 public:
  std::unique_ptr<FakeObserver> MakeObserver(const Timestep&) const override {
    return absl::make_unique<FakeObserver>();
  }
  void Aggregate(
      const Timestep& timestep,
      absl::Span<std::unique_ptr<FakeObserver> const> observers) override {
    for (auto& observer : observers) {
      for (auto iter : observer->agent_stats_) {
        PerAgent& per_agent = agent_stats_[iter.first];
        per_agent.observations += iter.second.observations;
        per_agent.outcomes += iter.second.outcomes;
      }
      for (auto iter : observer->location_stats_) {
        PerLocation& per_location = location_stats_[iter.first];
        per_location.observations += iter.second.observations;
        per_location.visits += iter.second.visits;
      }
    }
  }

  void CheckResults() {
    for (int i = 0; i < kNumAgents; ++i) {
      EXPECT_EQ(agent_stats_[i].observations, kNumSteps);
      EXPECT_EQ(agent_stats_[i].outcomes, (kNumSteps - 1) * kVisitsPerAgent);
    }
    for (int i = 0; i < kNumLocations; ++i) {
      EXPECT_EQ(location_stats_[i].observations, kNumSteps);
      EXPECT_EQ(location_stats_[i].visits, kNumSteps * kVisitsPerAgent);
    }
  }

 private:
  absl::flat_hash_map<int64, PerAgent> agent_stats_;
  absl::flat_hash_map<int64, PerLocation> location_stats_;
};

using SimBuilder = std::function<std::unique_ptr<Simulation>(
    absl::Time start, std::vector<std::unique_ptr<Agent>>,
    std::vector<std::unique_ptr<Location>>)>;

std::unique_ptr<Simulation> BuildSimulator(SimBuilder builder,
                                           OutcomeMap* outcomes,
                                           VisitMap* visits,
                                           ReportMap* reports) {
  std::vector<std::unique_ptr<Agent>> agents;
  for (int i = 0; i < kNumAgents; ++i) {
    agents.push_back(absl::make_unique<FakeAgent>(i, outcomes, reports));
  }
  std::vector<std::unique_ptr<Location>> locations;
  for (int i = 0; i < kNumLocations; ++i) {
    locations.push_back(absl::make_unique<FakeLocation>(i, visits));
  }
  auto sim =
      builder(absl::UnixEpoch(), std::move(agents), std::move(locations));
  return sim;
}

void CheckSimulatorResults(const OutcomeMap& outcomes, const VisitMap& visits,
                           const ReportMap& reports) {
  absl::MutexLock l(&map_mu);
  for (int i = 0; i < kNumAgents; i++) {
    auto outcome = outcomes.find(i);
    ASSERT_NE(outcome, outcomes.end());
    EXPECT_EQ(outcome->second, kVisitsPerAgent * (kNumSteps - 1));
    for (const int location : VisitLocations(i)) {
      auto visit = visits.find({location, i});
      ASSERT_NE(visit, visits.end());
      EXPECT_EQ(visit->second, kNumSteps);
    }
    for (const int to_agent : ReportRecipients(i)) {
      auto report = reports.find({i, to_agent});
      ASSERT_NE(report, reports.end());
      EXPECT_EQ(report->second, kNumSteps - 1);
    }
  }
}

TEST(SimulationTest, AllAgentsAndLocationsAreProcessedSerially) {
  OutcomeMap outcomes;
  VisitMap visits;
  ReportMap reports;
  auto sim = BuildSimulator(SerialSimulation, &outcomes, &visits, &reports);
  sim->Step(kNumSteps, absl::Hours(24));
  CheckSimulatorResults(outcomes, visits, reports);
}

TEST(SimulationTest, AllAgentsAndLocationsAreProcessedSeriallyStepByStep) {
  OutcomeMap outcomes;
  VisitMap visits;
  ReportMap reports;
  auto sim = BuildSimulator(SerialSimulation, &outcomes, &visits, &reports);
  for (int step = 0; step < kNumSteps; ++step) {
    sim->Step(1, absl::Hours(24));
  }
  CheckSimulatorResults(outcomes, visits, reports);
}

TEST(SimulationTest, AllAgentsAndLocationsAreProcessedSeriallyWithObserver) {
  OutcomeMap outcomes;
  VisitMap visits;
  ReportMap reports;

  auto sim = BuildSimulator(SerialSimulation, &outcomes, &visits, &reports);
  FakeObserverFactory observer_factory;
  sim->AddObserverFactory(&observer_factory);
  sim->Step(kNumSteps, absl::Hours(24));
  CheckSimulatorResults(outcomes, visits, reports);
  observer_factory.CheckResults();
}

TEST(SimulationTest, AllAgentsAndLocationsAreProcessedInParallel) {
  OutcomeMap outcomes;
  VisitMap visits;
  ReportMap reports;
  auto builder = [](absl::Time start, auto agents, auto locations) {
    return ParallelSimulation(start, std::move(agents), std::move(locations),
                              3);
  };
  auto sim = BuildSimulator(builder, &outcomes, &visits, &reports);
  sim->Step(kNumSteps, absl::Hours(24));
  CheckSimulatorResults(outcomes, visits, reports);
}

TEST(SimulationTest, AllAgentsAndLocationsAreProcessedInParallelStepByStep) {
  OutcomeMap outcomes;
  VisitMap visits;
  ReportMap reports;
  auto builder = [](absl::Time start, auto agents, auto locations) {
    return ParallelSimulation(start, std::move(agents), std::move(locations),
                              3);
  };
  auto sim = BuildSimulator(builder, &outcomes, &visits, &reports);
  for (int step = 0; step < kNumSteps; ++step) {
    sim->Step(1, absl::Hours(24));
  }
  CheckSimulatorResults(outcomes, visits, reports);
}

TEST(SimulationTest, AllAgentsAndLocationsAreProcessedInParallelWithObserver) {
  OutcomeMap outcomes;
  VisitMap visits;
  ReportMap reports;
  auto builder = [](absl::Time start, auto agents, auto locations) {
    return ParallelSimulation(start, std::move(agents), std::move(locations),
                              3);
  };
  auto sim = BuildSimulator(builder, &outcomes, &visits, &reports);
  FakeObserverFactory observer_factory;
  sim->AddObserverFactory(&observer_factory);
  sim->Step(kNumSteps, absl::Hours(24));
  CheckSimulatorResults(outcomes, visits, reports);
  observer_factory.CheckResults();
}

// TODO: Add a test for DistributedParallelSimulation using a mock
// DistributedManager.  Currently I'm relying on the stubby test.

}  // namespace
}  // namespace abesim
