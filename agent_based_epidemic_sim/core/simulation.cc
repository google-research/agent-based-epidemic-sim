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

#include <algorithm>
#include <memory>

#include "absl/base/thread_annotations.h"
#include "absl/container/fixed_array.h"
#include "absl/container/flat_hash_map.h"
#include "absl/memory/memory.h"
#include "absl/synchronization/mutex.h"
#include "absl/types/span.h"
#include "agent_based_epidemic_sim/core/broker.h"
#include "agent_based_epidemic_sim/core/event.h"
#include "agent_based_epidemic_sim/core/location.h"
#include "agent_based_epidemic_sim/core/observer.h"
#include "agent_based_epidemic_sim/core/timestep.h"
#include "agent_based_epidemic_sim/port/executor.h"
#include "agent_based_epidemic_sim/port/logging.h"

namespace abesim {

namespace {

const int kWorkChunkSize = 128;
const int kPerThreadBrokerBuffer = 256;

auto CompareUuid = [](const auto& a, const auto& b) {
  return a->uuid() < b->uuid();
};

int GetDestId(const Visit& visit) { return visit.location_uuid; }
int GetDestId(const InfectionOutcome& outcome) { return outcome.agent_uuid; }
int GetDestId(const ContactReport& report) { return report.to_agent_uuid; }

bool CompareDestId(const Visit& a, const Visit& b) {
  if (a.location_uuid != b.location_uuid) {
    return a.location_uuid < b.location_uuid;
  }
  if (a.start_time != b.start_time) {
    return a.start_time < b.start_time;
  }
  return a.agent_uuid < b.agent_uuid;
}
bool CompareDestId(const InfectionOutcome& a, const InfectionOutcome& b) {
  if (a.agent_uuid != b.agent_uuid) {
    return a.agent_uuid < b.agent_uuid;
  }
  return a.exposure.start_time < b.exposure.start_time;
}
bool CompareDestId(const ContactReport& a, const ContactReport& b) {
  if (a.to_agent_uuid != b.to_agent_uuid) {
    return a.to_agent_uuid < b.to_agent_uuid;
  }
  return a.from_agent_uuid < b.from_agent_uuid;
}

template <typename Msg>
void SortByDest(absl::Span<Msg> msgs) {
  std::sort(msgs.begin(), msgs.end(),
            [](const Msg& a, const Msg& b) { return CompareDestId(a, b); });
}

template <typename Msg>
std::pair<absl::Span<const Msg>, absl::Span<Msg>> SplitMessages(
    int64 uuid, absl::Span<Msg> messages) {
  DCHECK(messages.empty() || GetDestId(messages[0]) >= uuid)
      << "Message found for non-local entity.";
  int idx = 0;
  for (; idx < messages.size() && GetDestId(messages[idx]) == uuid; ++idx) {
  }
  return {messages.subspan(0, idx), messages.subspan(idx)};
}

class BaseSimulation : public Simulation {
 public:
  BaseSimulation(absl::Time start, std::vector<std::unique_ptr<Agent>> agents,
                 std::vector<std::unique_ptr<Location>> locations)
      : time_(start),
        agents_(std::move(agents)),
        locations_(std::move(locations)) {
    std::sort(agents_.begin(), agents_.end(), CompareUuid);
    std::sort(locations_.begin(), locations_.end(), CompareUuid);
  }

  void Step(const int steps, absl::Duration step_duration) final {
    Timestep timestep(time_, step_duration);
    for (int step = 0; step < steps; ++step) {
      RunAgentPhase(
          timestep,
          [&timestep](const absl::Span<const std::unique_ptr<Agent>> agents,
                      absl::Span<InfectionOutcome> outcomes,
                      absl::Span<ContactReport> reports,
                      ObserverShard* const observer,
                      Broker<Visit>* const visit_broker,
                      Broker<ContactReport>* const contact_report_broker) {
            SortByDest(outcomes);
            SortByDest(reports);
            for (const auto& agent : agents) {
              absl::Span<const InfectionOutcome> agent_outcomes;
              std::tie(agent_outcomes, outcomes) =
                  SplitMessages(agent->uuid(), outcomes);
              absl::Span<const ContactReport> agent_reports;
              std::tie(agent_reports, reports) =
                  SplitMessages(agent->uuid(), reports);
              observer->Observe(*agent, agent_outcomes);
              agent->ProcessInfectionOutcomes(timestep, agent_outcomes);
              agent->UpdateContactReports(timestep, agent_reports,
                                          contact_report_broker);
              agent->ComputeVisits(timestep, visit_broker);
            }
            DCHECK(outcomes.empty()) << "Unprocessed InfectionOutcomes";
            DCHECK(reports.empty()) << "Unprocessed ContactReports";
          });
      RunLocationPhase(
          timestep,
          [](const absl::Span<const std::unique_ptr<Location>> locations,
             absl::Span<Visit> visits, ObserverShard* const observer,
             Broker<InfectionOutcome>* const broker) {
            SortByDest(visits);
            for (const auto& location : locations) {
              absl::Span<const Visit> location_visits;
              std::tie(location_visits, visits) =
                  SplitMessages(location->uuid(), visits);
              observer->Observe(*location, location_visits);
              location->ProcessVisits(location_visits, broker);
            }
          });
      observer_manager_.AggregateForTimestep(timestep);
      timestep.Advance();
    }
    time_ = timestep.start_time();
  }

  using AgentPhaseFn = std::function<void(
      absl::Span<const std::unique_ptr<Agent>>, absl::Span<InfectionOutcome>,
      absl::Span<ContactReport>, ObserverShard* observer, Broker<Visit>*,
      Broker<ContactReport>*)>;
  using LocationPhaseFn = std::function<void(
      absl::Span<const std::unique_ptr<Location>>, absl::Span<Visit>,
      ObserverShard*, Broker<InfectionOutcome>*)>;

  virtual void RunAgentPhase(const Timestep& timestep,
                             const AgentPhaseFn& fn) = 0;
  virtual void RunLocationPhase(const Timestep& timestep,
                                const LocationPhaseFn& fn) = 0;

  void AddObserverFactory(ObserverFactoryBase* factory) override {
    observer_manager_.AddFactory(factory);
  }

  void RemoveObserverFactory(ObserverFactoryBase* factory) override {
    observer_manager_.RemoveFactory(factory);
  }

 protected:
  ObserverManager& GetObserverManager() { return observer_manager_; }
  absl::Span<const std::unique_ptr<Agent>> agents() { return agents_; }
  absl::Span<const std::unique_ptr<Location>> locations() { return locations_; }

 private:
  absl::Time time_;
  std::vector<std::unique_ptr<Agent>> agents_;
  std::vector<std::unique_ptr<Location>> locations_;
  class ObserverManager observer_manager_;
};

// A ConsumableBroker accumulates messages which can be consumed via the
// Consume method.
template <typename Msg>
class ConsumableBroker : public Broker<Msg> {
 private:
  struct Deleter {
    void operator()(std::vector<Msg>* const msgs) { broker->Delete(msgs); }
    ConsumableBroker* const broker;
  };
  virtual void Delete(std::vector<Msg>* const msgs) {
    DCHECK_EQ(msgs, &consume_);
    consume_.clear();
    // We are using swapping buffers so we're always reading from one
    // buffer and writing to another one.  For most of our message types
    // we don't read and write at the same time.  In that case we swap back
    // to using the buffer we consumed for the next round of sends to avoid
    // allocating any memory in the alternate buffer.
    if (send_.empty()) send_.swap(consume_);
  }

 public:
  void Send(const absl::Span<const Msg> msgs) override {
    send_.insert(send_.end(), msgs.begin(), msgs.end());
  }
  virtual std::unique_ptr<std::vector<Msg>, Deleter> Consume() {
    DCHECK(consume_.empty());
    consume_.swap(send_);
    return {&consume_, {this}};
  }

 private:
  std::vector<Msg> send_;
  std::vector<Msg> consume_;
};

// Serial implements a simulation that runs in a single thread.
class Serial : public BaseSimulation {
 public:
  Serial(absl::Time start, std::vector<std::unique_ptr<Agent>> agents,
         std::vector<std::unique_ptr<Location>> locations)
      : BaseSimulation(start, std::move(agents), std::move(locations)) {}

  void RunAgentPhase(const Timestep& timestep,
                     const AgentPhaseFn& fn) override {
    auto outcomes = outcome_broker_.Consume();
    auto reports = report_broker_.Consume();
    fn(agents(), absl::MakeSpan(*outcomes), absl::MakeSpan(*reports),
       GetObserverManager().MakeShard(timestep), &visit_broker_,
       &report_broker_);
  }
  void RunLocationPhase(const Timestep& timestep,
                        const LocationPhaseFn& fn) override {
    auto visits = visit_broker_.Consume();
    fn(locations(), absl::MakeSpan(*visits),
       GetObserverManager().MakeShard(timestep), &outcome_broker_);
  }

 private:
  ConsumableBroker<InfectionOutcome> outcome_broker_;
  ConsumableBroker<Visit> visit_broker_;
  ConsumableBroker<ContactReport> report_broker_;
};

// The Chunker helps divide a list of entities, and messages destined for those
// entities, into chunks of work.  Basically the first KWorkChunkSize entities
// and messages targeted at them goin in the first chunk and so on.
template <typename Entity>
class Chunker {
 public:
  explicit Chunker(const absl::Span<const std::unique_ptr<Entity>> entities)
      : chunks_((entities.size() + kWorkChunkSize - 1) / kWorkChunkSize) {
    size_t idx = 0;
    for (int chunk = 0; chunk < chunks_.size(); ++chunk) {
      chunks_[chunk] = entities.subspan(idx, kWorkChunkSize);
      int end = idx + chunks_[chunk].size();
      for (; idx < end; ++idx) {
        chunk_map_[entities[idx]->uuid()] = chunk;
      }
    }
  }

  template <typename Msg>
  int Chunk(const Msg& msg) const {
    auto iter = chunk_map_.find(GetDestId(msg));
    DCHECK(iter != chunk_map_.end());
    return iter->second;
  }
  absl::Span<const absl::Span<const std::unique_ptr<Entity>>> Chunks() const {
    return chunks_;
  }

 private:
  absl::FixedArray<absl::Span<const std::unique_ptr<Entity>>> chunks_;
  absl::flat_hash_map<int64, int> chunk_map_;
};

// WorkQueueBroker is the thread-safe analog to ConsumableBroker.  It can
// receive Send calls from any thread.
template <typename Entity, typename Msg>
class WorkQueueBroker : public Broker<Msg> {
 private:
  struct Deleter {
    void operator()(std::vector<std::vector<Msg>>* const msgs) {
      broker->Delete(msgs);
    }
    WorkQueueBroker* const broker;
  };
  virtual void Delete(std::vector<std::vector<Msg>>* const msgs) {
    absl::MutexLock l(&mu_);
    DCHECK_EQ(msgs, &consume_);
    std::for_each(consume_.begin(), consume_.end(), [](auto& v) { v.clear(); });
    // We are using swapping buffers so we're always reading from one
    // buffer and writing to another one.  For most of our message types
    // we don't read and write at the same time.  In that case we swap back
    // to using the buffer we consumed for the next round of sends to avoid
    // allocating any memory in the alternate buffer.  Otherwise we'll swap
    // back at the next call to Consume.
    if (!sent_msgs_) send_.swap(consume_);
  }

 public:
  explicit WorkQueueBroker(const Chunker<Entity>& chunker)
      : chunker_(chunker),
        send_(chunker.Chunks().size()),
        consume_(chunker.Chunks().size()) {}
  void Send(const absl::Span<const Msg> msgs) override {
    absl::MutexLock l(&mu_);
    for (const Msg& msg : msgs) {
      send_[chunker_.Chunk(msg)].push_back(msg);
    }
    sent_msgs_ = true;
  }
  virtual std::unique_ptr<std::vector<std::vector<Msg>>, Deleter> Consume() {
    absl::MutexLock l(&mu_);
    DCHECK(std::all_of(consume_.begin(), consume_.end(),
                       [](const std::vector<Msg>& v) { return v.empty(); }));
    sent_msgs_ = false;
    consume_.swap(send_);
    return {&consume_, {this}};
  }

 private:
  const Chunker<Entity>& chunker_;
  absl::Mutex mu_;
  bool sent_msgs_ = false;
  std::vector<std::vector<Msg>> send_ ABSL_GUARDED_BY(mu_);
  std::vector<std::vector<Msg>> consume_ ABSL_GUARDED_BY(mu_);
};

template <typename Worker>
void ParallelAgentPhase(const Timestep& timestep, Executor& executor,
                        ObserverManager& observer_manager,
                        const Chunker<Agent>& chunker,
                        std::vector<std::vector<InfectionOutcome>>& outcomes,
                        std::vector<std::vector<ContactReport>>& reports,
                        absl::FixedArray<Worker>& workers,
                        const BaseSimulation::AgentPhaseFn& fn) {
  absl::Mutex mu;
  int next_chunk = 0;

  absl::FixedArray<ObserverShard*> observers(workers.size());
  for (int i = 0; i < workers.size(); ++i) {
    observers[i] = observer_manager.MakeShard(timestep);
  }

  DCHECK_EQ(outcomes.size(), chunker.Chunks().size());
  DCHECK_EQ(reports.size(), chunker.Chunks().size());

  std::unique_ptr<Execution> exec = executor.NewExecution();
  for (int w = 0; w < workers.size(); ++w) {
    exec->Add([w, &workers, &outcomes, &reports, &chunker, &next_chunk, &mu,
               &observers, &fn]() {
      auto& worker = workers[w];
      while (true) {
        absl::Span<InfectionOutcome> my_outcomes;
        absl::Span<ContactReport> my_reports;
        absl::Span<const std::unique_ptr<Agent>> my_agents;
        {
          absl::MutexLock l(&mu);
          int chunk = next_chunk++;
          if (chunk >= chunker.Chunks().size()) break;
          my_agents = chunker.Chunks()[chunk];
          my_outcomes = absl::MakeSpan(outcomes[chunk]);
          my_reports = absl::MakeSpan(reports[chunk]);
        }
        fn(my_agents, my_outcomes, my_reports, observers[w],
           worker.visit_broker.get(), worker.report_broker.get());
      }
      worker.visit_broker->Flush();
      worker.report_broker->Flush();
    });
  }
  exec->Wait();
}

template <typename Worker>
void ParallelLocationPhase(const Timestep& timestep, Executor& executor,
                           ObserverManager& observer_manager,
                           const Chunker<Location>& chunker,
                           std::vector<std::vector<Visit>>& visits,
                           absl::FixedArray<Worker>& workers,
                           const BaseSimulation::LocationPhaseFn& fn) {
  absl::Mutex mu;
  int next_chunk = 0;

  absl::FixedArray<ObserverShard*> observers(workers.size());
  for (int i = 0; i < workers.size(); ++i) {
    observers[i] = observer_manager.MakeShard(timestep);
  }

  std::unique_ptr<Execution> exec = executor.NewExecution();
  for (int w = 0; w < workers.size(); ++w) {
    auto& worker = workers[w];
    exec->Add([w, &worker, &visits, &chunker, &next_chunk, &mu, &observers,
               &fn]() {
      while (true) {
        absl::Span<Visit> my_visits;
        absl::Span<const std::unique_ptr<Location>> my_locations;
        {
          absl::MutexLock l(&mu);
          int chunk = next_chunk++;
          if (chunk >= chunker.Chunks().size()) break;
          my_locations = chunker.Chunks()[chunk];
          my_visits = absl::MakeSpan(visits[chunk]);
        }
        fn(my_locations, my_visits, observers[w], worker.outcome_broker.get());
      }
      worker.outcome_broker->Flush();
    });
  }
  exec->Wait();
}

// Parallel implements a simulation that runs in multiple threads.
class Parallel : public BaseSimulation {
 public:
  Parallel(absl::Time start, std::vector<std::unique_ptr<Agent>> agents,
           std::vector<std::unique_ptr<Location>> locations,
           const int num_workers)
      : BaseSimulation(start, std::move(agents), std::move(locations)),
        executor_(NewExecutor(num_workers)),
        agent_chunker_(BaseSimulation::agents()),
        location_chunker_(BaseSimulation::locations()),
        agent_workers_(num_workers),
        location_workers_(num_workers),
        outcome_broker_(agent_chunker_),
        report_broker_(agent_chunker_),
        visit_broker_(location_chunker_) {
    for (int w = 0; w < num_workers; ++w) {
      agent_workers_[w].visit_broker =
          absl::make_unique<BufferingBroker<Visit>>(kPerThreadBrokerBuffer,
                                                    &visit_broker_);
      agent_workers_[w].report_broker =
          absl::make_unique<BufferingBroker<ContactReport>>(
              kPerThreadBrokerBuffer, &report_broker_);
      location_workers_[w].outcome_broker =
          absl::make_unique<BufferingBroker<InfectionOutcome>>(
              kPerThreadBrokerBuffer, &outcome_broker_);
    }
  }

  void RunAgentPhase(const Timestep& timestep,
                     const AgentPhaseFn& fn) override {
    auto outcomes = outcome_broker_.Consume();
    auto reports = report_broker_.Consume();
    ParallelAgentPhase(timestep, *executor_, GetObserverManager(),
                       agent_chunker_, *outcomes, *reports, agent_workers_, fn);
  }
  void RunLocationPhase(const Timestep& timestep,
                        const LocationPhaseFn& fn) override {
    auto visits = visit_broker_.Consume();
    ParallelLocationPhase(timestep, *executor_, GetObserverManager(),
                          location_chunker_, *visits, location_workers_, fn);
  }

 private:
  struct AgentWorker {
    std::unique_ptr<BufferingBroker<Visit>> visit_broker;
    std::unique_ptr<BufferingBroker<ContactReport>> report_broker;
  };
  struct LocationWorker {
    std::unique_ptr<BufferingBroker<InfectionOutcome>> outcome_broker;
  };

  std::unique_ptr<Executor> executor_;
  Chunker<Agent> agent_chunker_;
  Chunker<Location> location_chunker_;
  absl::FixedArray<AgentWorker> agent_workers_;
  absl::FixedArray<LocationWorker> location_workers_;
  WorkQueueBroker<Agent, InfectionOutcome> outcome_broker_;
  WorkQueueBroker<Agent, ContactReport> report_broker_;
  WorkQueueBroker<Location, Visit> visit_broker_;
};

// DistributedParallel implements a simulation that runs in multiple threads and
// interacts with distributed nodes also running simulations.
class DistributedParallel : public BaseSimulation {
 public:
  DistributedParallel(absl::Time start,
                      std::vector<std::unique_ptr<Agent>> agents,
                      std::vector<std::unique_ptr<Location>> locations,
                      const int num_workers,
                      DistributedManager* const distributed_manager)
      : BaseSimulation(start, std::move(agents), std::move(locations)),
        executor_(NewExecutor(num_workers)),
        agent_chunker_(BaseSimulation::agents()),
        location_chunker_(BaseSimulation::locations()),
        agent_workers_(num_workers),
        location_workers_(num_workers),
        outcome_broker_(agent_chunker_),
        report_broker_(agent_chunker_),
        visit_broker_(location_chunker_),
        distributed_manager_(distributed_manager) {
    for (int w = 0; w < num_workers; ++w) {
      agent_workers_[w].visit_broker =
          absl::make_unique<DistributingBroker<Visit>>(
              kPerThreadBrokerBuffer, distributed_manager->VisitMessenger(),
              &visit_broker_);
      agent_workers_[w].report_broker =
          absl::make_unique<DistributingBroker<ContactReport>>(
              kPerThreadBrokerBuffer,
              distributed_manager->ContactReportMessenger(), &report_broker_);
      location_workers_[w].outcome_broker =
          absl::make_unique<DistributingBroker<InfectionOutcome>>(
              kPerThreadBrokerBuffer, distributed_manager->OutcomeMessenger(),
              &outcome_broker_);
    }
  }

  ~DistributedParallel() override {
    distributed_manager_->VisitMessenger()->SetReceiveBrokerForNextPhase(
        nullptr);
    distributed_manager_->ContactReportMessenger()
        ->SetReceiveBrokerForNextPhase(nullptr);
    distributed_manager_->OutcomeMessenger()->SetReceiveBrokerForNextPhase(
        nullptr);
  }

  void RunAgentPhase(const Timestep& timestep,
                     const AgentPhaseFn& fn) override {
    auto outcomes = outcome_broker_.Consume();
    auto reports = report_broker_.Consume();

    distributed_manager_->VisitMessenger()->SetReceiveBrokerForNextPhase(
        &visit_broker_);
    distributed_manager_->ContactReportMessenger()
        ->SetReceiveBrokerForNextPhase(&report_broker_);

    ParallelAgentPhase(timestep, *executor_, GetObserverManager(),
                       agent_chunker_, *outcomes, *reports, agent_workers_, fn);
    distributed_manager_->VisitMessenger()->FlushAndAwaitRemotes();
    // TODO: We technically don't need to await remotes here, but we
    // should flush.  Consider splitting the two functions and calling
    // await remotes at the end of the location phase.
    distributed_manager_->ContactReportMessenger()->FlushAndAwaitRemotes();
  }
  void RunLocationPhase(const Timestep& timestep,
                        const LocationPhaseFn& fn) override {
    auto visits = visit_broker_.Consume();
    distributed_manager_->OutcomeMessenger()->SetReceiveBrokerForNextPhase(
        &outcome_broker_);
    ParallelLocationPhase(timestep, *executor_, GetObserverManager(),
                          location_chunker_, *visits, location_workers_, fn);
    distributed_manager_->OutcomeMessenger()->FlushAndAwaitRemotes();
  }

 private:
  struct AgentWorker {
    std::unique_ptr<DistributingBroker<Visit>> visit_broker;
    std::unique_ptr<DistributingBroker<ContactReport>> report_broker;
  };
  struct LocationWorker {
    std::unique_ptr<DistributingBroker<InfectionOutcome>> outcome_broker;
  };

  std::unique_ptr<Executor> executor_;
  Chunker<Agent> agent_chunker_;
  Chunker<Location> location_chunker_;
  absl::FixedArray<AgentWorker> agent_workers_;
  absl::FixedArray<LocationWorker> location_workers_;
  WorkQueueBroker<Agent, InfectionOutcome> outcome_broker_;
  WorkQueueBroker<Agent, ContactReport> report_broker_;
  WorkQueueBroker<Location, Visit> visit_broker_;
  DistributedManager* const distributed_manager_;
};

}  // namespace

std::unique_ptr<Simulation> SerialSimulation(
    absl::Time start, std::vector<std::unique_ptr<Agent>> agents,
    std::vector<std::unique_ptr<Location>> locations) {
  return absl::make_unique<Serial>(start, std::move(agents),
                                   std::move(locations));
}

std::unique_ptr<Simulation> ParallelSimulation(
    absl::Time start, std::vector<std::unique_ptr<Agent>> agents,
    std::vector<std::unique_ptr<Location>> locations, const int num_workers) {
  return absl::make_unique<Parallel>(start, std::move(agents),
                                     std::move(locations), num_workers);
}

std::unique_ptr<Simulation> ParallelDistributedSimulation(
    absl::Time start, std::vector<std::unique_ptr<Agent>> agents,
    std::vector<std::unique_ptr<Location>> locations,
    const int num_local_workers,
    DistributedManager* const distributed_manager) {
  return absl::make_unique<DistributedParallel>(
      start, std::move(agents), std::move(locations), num_local_workers,
      distributed_manager);
}

}  // namespace abesim
