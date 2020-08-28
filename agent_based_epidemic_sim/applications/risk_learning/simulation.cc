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
#include "absl/status/status.h"
#include "absl/strings/str_cat.h"
#include "absl/strings/str_join.h"
#include "absl/strings/string_view.h"
#include "agent_based_epidemic_sim/agent_synthesis/population_profile.pb.h"
#include "agent_based_epidemic_sim/applications/risk_learning/config.pb.h"
#include "agent_based_epidemic_sim/applications/risk_learning/risk_score.h"
#include "agent_based_epidemic_sim/core/agent.h"
#include "agent_based_epidemic_sim/core/duration_specified_visit_generator.h"
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

absl::Status RunSimulation(const RiskLearningSimulationConfig& config,
                           int num_workers) {
  // location_types is filled during location loading and is const after that.
  // This is important as it may be accessed by multiple threads during the
  // run of the simulation.
  absl::flat_hash_map<int64, LocationReference::Type> location_types;
  LocationTypeFn get_location_type = [&location_types](int64 uuid) {
    return location_types[uuid];
  };

  // TODO: Load these values from config.
  MicroExposureGenerator micro_generator(
      {ProximityTrace({1.0f, 3.0f, 9.0f, 3.0f})});

  // Read in locations.
  std::vector<std::unique_ptr<Location>> locations;
  std::vector<std::pair<int64, int64>> edges;
  for (const std::string& location_file : config.location_file()) {
    auto reader = MakeRecordReader(location_file);
    LocationProto proto;
    while (reader.ReadRecord(proto)) {
      location_types[proto.reference().uuid()] = proto.reference().type();
      switch (proto.location_case()) {
        case LocationProto::kGraph:
          edges.clear();
          edges.reserve(proto.graph().edges_size());
          for (const GraphLocation::Edge& edge : proto.graph().edges()) {
            edges.push_back({edge.uuid_a(), edge.uuid_b()});
          }
          locations.push_back(NewGraphLocation(proto.reference().uuid(), 0.0,
                                               edges, micro_generator));
          break;
        default:
          return absl::InvalidArgumentError(
              absl::StrCat("Invalid location type: ", proto.DebugString()));
      }
    }
    absl::Status status = reader.status();
    if (!status.ok()) return status;
    reader.Close();
  }

  // TODO: Specify parameters explicitly here.
  HazardTransmissionModel transmission;
  // Read in population profiles.
  absl::flat_hash_map<int, PopulationProfileData> profile_data;
  for (const PopulationProfile& profile : config.profiles()) {
    profile_data[profile.id()] = {
        .profile = &profile,
    };
  }

  absl::flat_hash_map<std::string, std::unique_ptr<VisitGenerator>>
      visit_gen_cache;

  // Read in agents.
  std::vector<std::unique_ptr<Agent>> agents;
  for (const std::string& agent_file : config.agent_file()) {
    auto reader = MakeRecordReader(agent_file);
    AgentProto proto;
    while (reader.ReadRecord(proto)) {
      auto profile_iter = profile_data.find(proto.population_profile_id());
      if (profile_iter == profile_data.end()) {
        return absl::InvalidArgumentError(absl::StrCat(
            "Invalid population profile id for agent: ", proto.DebugString()));
      }
      // TODO: It is wasteful that we are making a new transition model
      // for each agent.  To fix this we need to make GetNextHealthTransition
      // thread safe.  This is complicated by the fact that
      // absl::discrete_distribution::operator() is non-const.
      auto risk_score_or =
          CreateLearningRiskScore(config.tracing_policy(), get_location_type);
      if (!risk_score_or.ok()) return risk_score_or.status();
      agents.push_back(SEIRAgent::CreateSusceptible(
          proto.uuid(), &transmission,
          PTTSTransitionModel::CreateFromProto(
              profile_iter->second.profile->transition_model()),
          GetVisitGenerator(proto, *profile_iter->second.profile,
                            visit_gen_cache),
          std::move(*risk_score_or)));
    }
    absl::Status status = reader.status();
    if (!status.ok()) return status;
    reader.Close();
  }

  auto time_or = DecodeGoogleApiProto(config.init_time());
  if (!time_or.ok()) return time_or.status();
  const auto init_time = time_or.value();
  auto step_size_or = DecodeGoogleApiProto(config.step_size());
  if (!step_size_or.ok()) return step_size_or.status();
  const auto step_size = step_size_or.value();

  auto sim = num_workers > 1
                 ? ParallelSimulation(init_time, std::move(agents),
                                      std::move(locations), num_workers)
                 : SerialSimulation(init_time, std::move(agents),
                                    std::move(locations));
  sim->Step(config.steps(), step_size);
  return absl::OkStatus();
}

}  // namespace abesim
