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

#include "agent_based_epidemic_sim/applications/home_work/simulation.h"

#include <queue>
#include <string>

#include "absl/strings/string_view.h"
#include "absl/time/time.h"
#include "agent_based_epidemic_sim/agent_synthesis/agent_sampler.h"
#include "agent_based_epidemic_sim/agent_synthesis/population_profile.pb.h"
#include "agent_based_epidemic_sim/agent_synthesis/shuffled_sampler.h"
#include "agent_based_epidemic_sim/applications/home_work/config.pb.h"
#include "agent_based_epidemic_sim/applications/home_work/learning_contacts_observer.h"
#include "agent_based_epidemic_sim/applications/home_work/learning_history_and_testing_observer.h"
#include "agent_based_epidemic_sim/applications/home_work/observer.h"
#include "agent_based_epidemic_sim/applications/home_work/risk_score.h"
#include "agent_based_epidemic_sim/core/agent.h"
#include "agent_based_epidemic_sim/core/aggregated_transmission_model.h"
#include "agent_based_epidemic_sim/core/duration_specified_visit_generator.h"
#include "agent_based_epidemic_sim/core/location.h"
#include "agent_based_epidemic_sim/core/location_discrete_event_simulator.h"
#include "agent_based_epidemic_sim/core/micro_exposure_generator_builder.h"
#include "agent_based_epidemic_sim/core/ptts_transition_model.h"
#include "agent_based_epidemic_sim/core/risk_score.h"
#include "agent_based_epidemic_sim/core/seir_agent.h"
#include "agent_based_epidemic_sim/core/simulation.h"
#include "agent_based_epidemic_sim/core/uuid_generator.h"
#include "agent_based_epidemic_sim/core/wrapped_transition_model.h"
#include "agent_based_epidemic_sim/port/file_utils.h"
#include "agent_based_epidemic_sim/port/logging.h"
#include "agent_based_epidemic_sim/port/time_proto_util.h"

namespace abesim {
namespace {
constexpr char kBusinessAlpha[] = "business_alpha";
constexpr char kBusinessBeta[] = "business_beta";
constexpr char kHouseholdSize[] = "household_size";
constexpr char kHouseholdSizeProbability[] = "household_size_probability";
constexpr char kDistancingStageStart[] = "distancing_stage_start";
constexpr char kDistancingStageFraction[] =
    "distancing_stage_essential_workers";
constexpr int64 kPopulationProfileId = 0;
constexpr char kTopBusiness[] = "top_business_size";
constexpr int kNumTopBusinesses = 5;
}  // namespace

namespace {

int64 GetLocationUuidForTypeOrDie(const AgentProto& agent,
                                  const LocationReference::Type type) {
  for (const auto& location : agent.locations()) {
    if (location.type() == type) {
      return location.uuid();
    }
  }
  LOG(FATAL) << "Location not found for type: " << type;
}

std::vector<LocationDuration> GetLocationDurations(
    absl::BitGen* gen, const AgentProto& agent,
    const PopulationProfile& population_profile) {
  std::vector<LocationDuration> durations;
  durations.reserve(population_profile.visit_durations_size());
  for (const VisitDuration& visit_duration :
       population_profile.visit_durations()) {
    durations.push_back(
        {.location_uuid =
             GetLocationUuidForTypeOrDie(agent, visit_duration.location_type()),
         .sample_duration =
             [gen, mean = visit_duration.gaussian_distribution().mean(),
              stddev = visit_duration.gaussian_distribution().stddev()](
                 float adjustment) {
               return absl::Gaussian<float>(*gen, mean * adjustment, stddev);
             }});
  }
  return durations;
}

std::vector<std::pair<std::string, std::string>> GetHomeWorkPassthrough(
    const HomeWorkSimulationConfig& config,
    const std::vector<LocationProto> locations) {
  std::vector<std::pair<std::string, std::string>> passthrough;
  for (const auto& distancing_stage : config.distancing_policy().stages()) {
    passthrough.push_back(
        {kDistancingStageStart,
         std::to_string(distancing_stage.start_time().seconds())});
    passthrough.push_back(
        {kDistancingStageFraction,
         std::to_string(distancing_stage.essential_worker_fraction())});
  }

  passthrough.push_back(
      {kBusinessAlpha,
       std::to_string(
           config.location_distributions().business_distribution().alpha())});
  passthrough.push_back(
      {kBusinessBeta,
       std::to_string(
           config.location_distributions().business_distribution().beta())});

  for (const auto& household_size : config.location_distributions()
                                        .household_size_distribution()
                                        .buckets()) {
    passthrough.push_back(
        {kHouseholdSize, std::to_string(household_size.int_value())});
    passthrough.push_back(
        {kHouseholdSizeProbability, std::to_string(household_size.count())});
  }
  for (const auto& health_state_probability :
       config.agent_properties()
           .initial_health_state_distribution()
           .buckets()) {
    HealthState state;
    health_state_probability.proto_value().UnpackTo(&state);
    passthrough.push_back(
        {absl::StrCat("initial_prob_", HealthState::State_Name(state.state())),
         std::to_string(health_state_probability.count())});
  }
  std::priority_queue<int, std::vector<int>, std::greater<int>> top_businesses;
  for (const LocationProto& location : locations) {
    if (top_businesses.size() < kNumTopBusinesses) {
      top_businesses.push(location.dense().size());
    } else {
      if (top_businesses.top() < location.dense().size()) {
        top_businesses.pop();
        top_businesses.push(location.dense().size());
      }
    }
  }
  for (int i = 0; i < kNumTopBusinesses && !top_businesses.empty(); ++i) {
    passthrough.push_back({kTopBusiness, std::to_string(top_businesses.top())});
    top_businesses.pop();
  }
  return passthrough;
}

void AddVisitDurationDistribution(const VisitDurationDistribution& distribution,
                                  const LocationReference::Type location_type,
                                  PopulationProfile* population_profile) {
  auto visit_duration = population_profile->add_visit_durations();
  visit_duration->set_location_type(location_type);
  visit_duration->mutable_gaussian_distribution()->set_mean(
      distribution.mean());
  visit_duration->mutable_gaussian_distribution()->set_stddev(
      distribution.stddev());
}

}  // namespace

// Next steps:
// 1. (Improve) Mapping of uuid to location properties.
// 2. (Improve) Mapping of uuid to index-in-array.
// 3. Injection of other infection outcomes (noise) per step.
// 4. Other visit types, sample from random.
// 5. Seasonality of business, transmissibility by location.
// 6. Population class-structured attributes.
// Also:
// 7. Distributed initialization.
SimulationContext GetSimulationContext(const HomeWorkSimulationConfig& config) {
  LOG(INFO) << "Building agents and locations from config: "
            << config.DebugString();
  // Samples the locations and agents.
  const int64 kUuidShard = 0LL;
  SimulationContext context;
  std::vector<LocationProto>& locations = context.locations;
  auto uuid_generator =
      absl::make_unique<ShardedGlobalIdUuidGenerator>(kUuidShard);
  auto business_sampler = MakeBusinessSampler(
      config.location_distributions().business_distribution(),
      config.population_size(), *uuid_generator, &locations);
  auto household_sampler = MakeHouseholdSampler(
      config.location_distributions().household_size_distribution(),
      config.population_size(), *uuid_generator, &locations);
  auto health_state_sampler = HealthStateSampler::FromProto(
      config.agent_properties().initial_health_state_distribution());
  auto samplers = absl::WrapUnique(
      new Samplers({{absl::optional<std::unique_ptr<ShuffledSampler>>(),
                     absl::optional<std::unique_ptr<ShuffledSampler>>(
                         std::move(household_sampler)),
                     absl::optional<std::unique_ptr<ShuffledSampler>>(
                         std::move(business_sampler))}}));
  PopulationProfiles& population_profiles = context.population_profiles;
  auto population_profile = population_profiles.add_population_profiles();
  population_profile->set_id(kPopulationProfileId);
  *population_profile->mutable_transition_model() =
      config.agent_properties().ptts_transition_model();
  population_profile->set_susceptibility(1);
  population_profile->set_infectiousness(1);
  AddVisitDurationDistribution(
      config.agent_properties().departure_distribution(),
      LocationReference::HOUSEHOLD, population_profile);
  AddVisitDurationDistribution(
      config.agent_properties().work_duration_distribution(),
      LocationReference::BUSINESS, population_profile);
  AddVisitDurationDistribution(config.agent_properties().arrival_distribution(),
                               LocationReference::HOUSEHOLD,
                               population_profile);
  ShuffledLocationAgentSampler sampler(std::move(samplers),
                                       std::move(uuid_generator),
                                       std::move(health_state_sampler));
  context.agents.reserve(config.population_size());
  for (int i = 0; i < config.population_size(); ++i) {
    context.agents.push_back(sampler.Next());
  }
  absl::flat_hash_set<int64> business_uuids;
  std::for_each(
      context.locations.begin(), context.locations.end(),
      [&business_uuids](const auto& location) {
        if (location.reference().type() == LocationReference::BUSINESS) {
          business_uuids.insert(location.reference().uuid());
        }
      });
  context.location_type = [business_uuids =
                               std::move(business_uuids)](int64 uuid) {
    return business_uuids.contains(uuid) ? LocationType::kWork
                                         : LocationType::kHome;
  };
  return context;
}

void RunSimulation(
    absl::string_view output_file_path, absl::string_view learning_output_base,
    const HomeWorkSimulationConfig& config,
    const std::function<std::unique_ptr<RiskScoreGenerator>(LocationTypeFn)>&
        get_risk_score_generator,
    const int num_workers, const SimulationContext& context) {
  LOG(INFO) << "Writing output to file: " << output_file_path;

  auto time_or = DecodeGoogleApiProto(config.init_time());
  CHECK(time_or.ok());
  const auto init_time = time_or.value();
  auto step_size_or = DecodeGoogleApiProto(config.step_size());
  CHECK(step_size_or.ok());
  const auto step_size = step_size_or.value();

  auto transmission_model =
      absl::make_unique<AggregatedTransmissionModel>(config.transmissibility());
  absl::FixedArray<std::unique_ptr<TransitionModel>> transition_models(
      context.population_profiles.population_profiles_size());
  for (int i = 0; i < transition_models.size(); ++i) {
    transition_models[i] = PTTSTransitionModel::CreateFromProto(
        context.population_profiles.population_profiles(i).transition_model());
  }
  auto policy_generator = get_risk_score_generator(context.location_type);
  std::vector<std::unique_ptr<Agent>> seir_agents;
  seir_agents.reserve(context.agents.size());
  absl::BitGen gen;
  for (const auto& agent : context.agents) {
    seir_agents.push_back(SEIRAgent::Create(
        agent.uuid(),
        {.time = init_time, .health_state = agent.initial_health_state()},
        transmission_model.get(),
        absl::make_unique<WrappedTransitionModel>(
            transition_models[agent.population_profile_id()].get()),
        absl::make_unique<DurationSpecifiedVisitGenerator>(GetLocationDurations(
            &gen, agent,
            context.population_profiles.population_profiles(
                agent.population_profile_id()))),
        policy_generator->NextRiskScore()));
  }
  MicroExposureGeneratorBuilder meg_builder;
  std::vector<std::unique_ptr<Location>> location_des;
  location_des.reserve(context.locations.size());
  for (const auto& location : context.locations) {
    // TODO: Load a ProximityTrace Distribution from file.
    location_des.push_back(absl::make_unique<LocationDiscreteEventSimulator>(
        location.reference().uuid(),
        meg_builder.Build(kNonParametricTraceDistribution)));
  }
  // Initializes Simulation.
  auto sim = num_workers > 1
                 ? ParallelSimulation(init_time, std::move(seir_agents),
                                      std::move(location_des), num_workers)
                 : SerialSimulation(init_time, std::move(seir_agents),
                                    std::move(location_des));

  std::vector<std::pair<std::string, std::string>> passthrough =
      GetHomeWorkPassthrough(config, context.locations);
  // TODO: Check if file exists.
  std::unique_ptr<file::FileWriter> output_file =
      file::OpenOrDie(output_file_path);
  HomeWorkSimulationObserverFactory observer_factory(
      output_file.get(), context.location_type, passthrough);
  sim->AddObserverFactory(&observer_factory);
  LearningContactsObserverFactory learning_contacts_observer_factory(
      learning_output_base);
  if (!learning_output_base.empty()) {
    sim->AddObserverFactory(&learning_contacts_observer_factory);
  }
  sim->Step(config.num_steps() - 1, step_size);
  // Do the last step to get agent history and tests.
  LearningHistoryAndTestingObserverFactory hist_and_test_observer_factory(
      learning_output_base);
  if (!learning_output_base.empty()) {
    sim->AddObserverFactory(&hist_and_test_observer_factory);
  }
  sim->Step(1, step_size);
  LOG(INFO) << observer_factory.status();
  LOG(INFO) << learning_contacts_observer_factory.status();
  LOG(INFO) << hist_and_test_observer_factory.status();
  CHECK_EQ(absl::OkStatus(), output_file->Close());
}

void RunSimulation(absl::string_view output_file_path,
                   absl::string_view mpi_learning_output_base,
                   const HomeWorkSimulationConfig& config,
                   const int num_workers) {
  auto get_risk_score_generator = [&config](LocationTypeFn location_type) {
    return *NewRiskScoreGenerator(config.distancing_policy(), location_type);
  };
  auto context = GetSimulationContext(config);
  RunSimulation(output_file_path, mpi_learning_output_base, config,
                get_risk_score_generator, num_workers, context);
}

}  // namespace abesim
