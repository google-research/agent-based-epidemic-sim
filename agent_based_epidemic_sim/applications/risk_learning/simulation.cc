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

#include "agent_based_epidemic_sim/applications/risk_learning/simulation.h"

#include <fcntl.h>

#include <memory>
#include <string>
#include <vector>

#include "absl/container/fixed_array.h"
#include "absl/container/flat_hash_map.h"
#include "absl/memory/memory.h"
#include "absl/status/status.h"
#include "absl/strings/str_cat.h"
#include "absl/strings/str_join.h"
#include "absl/strings/string_view.h"
#include "agent_based_epidemic_sim/agent_synthesis/population_profile.pb.h"
#include "agent_based_epidemic_sim/applications/risk_learning/config.pb.h"
#include "agent_based_epidemic_sim/applications/risk_learning/risk_score.h"
#include "agent_based_epidemic_sim/core/agent.h"
#include "agent_based_epidemic_sim/core/duration_specified_visit_generator.h"
#include "agent_based_epidemic_sim/core/event.h"
#include "agent_based_epidemic_sim/core/graph_location.h"
#include "agent_based_epidemic_sim/core/hazard_transmission_model.h"
#include "agent_based_epidemic_sim/core/micro_exposure_generator.h"
#include "agent_based_epidemic_sim/core/parameter_distribution.pb.h"
#include "agent_based_epidemic_sim/core/ptts_transition_model.h"
#include "agent_based_epidemic_sim/core/random.h"
#include "agent_based_epidemic_sim/core/risk_score.h"
#include "agent_based_epidemic_sim/core/seir_agent.h"
#include "agent_based_epidemic_sim/core/simulation.h"
#include "agent_based_epidemic_sim/core/transition_model.h"
#include "agent_based_epidemic_sim/core/transmission_model.h"
#include "agent_based_epidemic_sim/core/visit_generator.h"
#include "agent_based_epidemic_sim/port/time_proto_util.h"
#include "agent_based_epidemic_sim/util/records.h"

namespace abesim {
namespace {

struct PopulationProfileData {
  const PopulationProfile* profile;
};

int64 GetLocationUuidForTypeOrDie(const AgentProto& agent,
                                  const LocationReference::Type type) {
  for (const auto& location : agent.locations()) {
    if (location.type() == type) {
      return location.uuid();
    }
  }
  LOG(FATAL) << "Location not found for type: " << type;
}

const VisitGenerator& GetVisitGenerator(
    const AgentProto& agent, const PopulationProfile& profile,
    absl::flat_hash_map<std::string, std::unique_ptr<VisitGenerator>>& cache) {
  // Agents with the same set of locations and the same profile can share
  // a visit generator.
  std::string key = absl::StrCat(agent.population_profile_id());
  for (const LocationReference& ref : agent.locations()) {
    absl::StrAppend(&key, ",", ref.uuid());
  }
  std::unique_ptr<VisitGenerator>& value = cache[key];
  if (value == nullptr) {
    std::vector<LocationDuration> durations;
    durations.reserve(profile.visit_durations_size());
    for (const VisitDuration& visit_duration : profile.visit_durations()) {
      durations.push_back(
          {.location_uuid = GetLocationUuidForTypeOrDie(
               agent, visit_duration.location_type()),
           .sample_duration =
               [mean = visit_duration.gaussian_distribution().mean(),
                stddev = visit_duration.gaussian_distribution().stddev()](
                   float adjustment) {
                 return absl::Gaussian<float>(GetBitGen(), mean * adjustment,
                                              stddev);
               }});
    }
    value = absl::make_unique<DurationSpecifiedVisitGenerator>(durations);
  }
  return *value;
}

}  // namespace

class RiskLearningSimulation : public Simulation {
 public:
  void Step(int steps, absl::Duration step_duration) override {
    sim_->Step(steps, step_duration);
  }
  void AddObserverFactory(ObserverFactoryBase* factory) override {
    sim_->AddObserverFactory(factory);
  }
  void RemoveObserverFactory(ObserverFactoryBase* factory) override {
    sim_->RemoveObserverFactory(factory);
  }

  static absl::StatusOr<std::unique_ptr<Simulation>> Build(
      const RiskLearningSimulationConfig& config, int num_workers) {
    auto result = absl::WrapUnique(new RiskLearningSimulation);

    // TODO: Load these values from config.
    std::vector<ProximityTrace> proximity_traces = {
        ProximityTrace({1.0f, 3.0f, 9.0f, 3.0f})};
    result->micro_generator_ =
        absl::make_unique<MicroExposureGenerator>(proximity_traces);

    // Read in locations.
    std::vector<std::unique_ptr<Location>> locations;
    int i = 0;
    for (const std::string& location_file : config.location_file()) {
      auto reader = MakeRecordReader(location_file);
      LocationProto proto;
      while (reader.ReadRecord(proto)) {
        result->location_types_[proto.reference().uuid()] =
            proto.reference().type();
        switch (proto.location_case()) {
          case LocationProto::kGraph: {
            std::vector<std::pair<int64, int64>> edges;
            edges.reserve(proto.graph().edges_size());
            for (const GraphLocation::Edge& edge : proto.graph().edges()) {
              edges.push_back({edge.uuid_a(), edge.uuid_b()});
            }
            locations.push_back(NewGraphLocation(proto.reference().uuid(), 0.0,
                                                 std::move(edges),
                                                 *result->micro_generator_));
            break;
          }
          default:
            return absl::InvalidArgumentError(absl::StrCat(
                "Invalid location ", i, ": ", proto.DebugString()));
        }
        i++;
      }
      absl::Status status = reader.status();
      if (!status.ok()) return status;
      reader.Close();
    }

    // TODO: Specify parameters explicitly here.
    result->transmission_model_ = absl::make_unique<HazardTransmissionModel>();
    // Read in population profiles.
    absl::flat_hash_map<int, PopulationProfileData> profile_data;
    for (const PopulationProfile& profile : config.profiles()) {
      profile_data[profile.id()] = {
          .profile = &profile,
      };
    }

    // Read in agents.
    std::vector<std::unique_ptr<Agent>> agents;
    for (const std::string& agent_file : config.agent_file()) {
      auto reader = MakeRecordReader(agent_file);
      AgentProto proto;
      while (reader.ReadRecord(proto)) {
        auto profile_iter = profile_data.find(proto.population_profile_id());
        if (profile_iter == profile_data.end()) {
          return absl::InvalidArgumentError(
              absl::StrCat("Invalid population profile id for agent: ",
                           proto.DebugString()));
        }
        // TODO: It is wasteful that we are making a new transition model
        // for each agent.  To fix this we need to make GetNextHealthTransition
        // thread safe.  This is complicated by the fact that
        // absl::discrete_distribution::operator() is non-const.
        auto risk_score_or = CreateLearningRiskScore(
            config.tracing_policy(), result->get_location_type_);
        if (!risk_score_or.ok()) return risk_score_or.status();
        agents.push_back(SEIRAgent::CreateSusceptible(
            proto.uuid(), result->transmission_model_.get(),
            PTTSTransitionModel::CreateFromProto(
                profile_iter->second.profile->transition_model()),
            GetVisitGenerator(proto, *profile_iter->second.profile,
                              result->visit_gen_cache_),
            std::move(*risk_score_or)));
      }
      absl::Status status = reader.status();
      if (!status.ok()) return status;
      reader.Close();
    }

    auto time_or = DecodeGoogleApiProto(config.init_time());
    if (!time_or.ok()) return time_or.status();
    const auto init_time = time_or.value();

    result->sim_ = num_workers > 1
                       ? ParallelSimulation(init_time, std::move(agents),
                                            std::move(locations), num_workers)
                       : SerialSimulation(init_time, std::move(agents),
                                          std::move(locations));
    return std::move(result);
  }

 private:
  RiskLearningSimulation()
      : get_location_type_(
            [this](int64 uuid) { return location_types_[uuid]; }) {}

  std::unique_ptr<MicroExposureGenerator> micro_generator_;
  std::unique_ptr<HazardTransmissionModel> transmission_model_;
  absl::flat_hash_map<int64, LocationReference::Type> location_types_;
  const LocationTypeFn get_location_type_;
  // location_types is filled during location loading in the constructor and is
  // const after that.
  // This is important as it may be accessed by multiple threads during the
  // run of the simulation.
  absl::flat_hash_map<std::string, std::unique_ptr<VisitGenerator>>
      visit_gen_cache_;
  std::unique_ptr<Simulation> sim_;
};

absl::StatusOr<std::unique_ptr<Simulation>> BuildSimulation(
    const RiskLearningSimulationConfig& config, int num_workers) {
  return RiskLearningSimulation::Build(config, num_workers);
}

absl::Status RunSimulation(const RiskLearningSimulationConfig& config,
                           int num_workers) {
  auto sim_or = BuildSimulation(config, num_workers);
  if (!sim_or.ok()) return sim_or.status();
  auto step_size_or = DecodeGoogleApiProto(config.step_size());
  if (!step_size_or.ok()) return step_size_or.status();
  const auto step_size = step_size_or.value();
  sim_or.value()->Step(config.steps(), step_size);
  return absl::OkStatus();
}

}  // namespace abesim
