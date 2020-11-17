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
#include <random>
#include <string>
#include <type_traits>
#include <vector>

#include "absl/container/fixed_array.h"
#include "absl/container/flat_hash_map.h"
#include "absl/flags/flag.h"
#include "absl/functional/bind_front.h"
#include "absl/memory/memory.h"
#include "absl/random/bit_gen_ref.h"
#include "absl/status/status.h"
#include "absl/status/statusor.h"
#include "absl/strings/str_cat.h"
#include "absl/strings/str_join.h"
#include "absl/strings/string_view.h"
#include "agent_based_epidemic_sim/agent_synthesis/population_profile.pb.h"
#include "agent_based_epidemic_sim/applications/risk_learning/config.pb.h"
#include "agent_based_epidemic_sim/applications/risk_learning/infectivity_model.h"
#include "agent_based_epidemic_sim/applications/risk_learning/observers.h"
#include "agent_based_epidemic_sim/applications/risk_learning/risk_score.h"
#include "agent_based_epidemic_sim/applications/risk_learning/triple_exposure_generator.h"
#include "agent_based_epidemic_sim/applications/risk_learning/triple_exposure_generator_builder.h"
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
#include "agent_based_epidemic_sim/core/visit.h"
#include "agent_based_epidemic_sim/core/visit_generator.h"
#include "agent_based_epidemic_sim/port/executor.h"
#include "agent_based_epidemic_sim/port/time_proto_util.h"
#include "agent_based_epidemic_sim/util/records.h"

ABSL_FLAG(int, num_reader_threads, 16,
          "Number of agent and location reader threads to use.");

namespace abesim {
namespace {

struct PopulationProfileData {
  const PopulationProfile* profile;
  std::negative_binomial_distribution<int> random_edges_distribution;
};

// Generates visits to an agent's configured locations lasting a
// profile-dependent duration, and having a profile-dependent susceptibility.
class RiskLearningVisitGenerator : public VisitGenerator {
 public:
  RiskLearningVisitGenerator(const AgentProto& agent,
                             PopulationProfileData& profile)
      : generator_(absl::make_unique<DurationSpecifiedVisitGenerator>(
            GetLocationDurations(agent, *profile.profile))),
        susceptibility_(profile.profile->susceptibility()),
        visit_dynamics_(GenerateVisitDynamics(profile)) {}

  void GenerateVisits(const Timestep& timestep, const RiskScore& risk_score,
                      std::vector<Visit>* visits) const final {
    int i = visits->size();  // Index of first element added below.
    generator_->GenerateVisits(timestep, risk_score, visits);
    for (; i < visits->size(); ++i) {
      Visit& visit = (*visits)[i];
      visit.susceptibility = susceptibility_;
      visit.location_dynamics = visit_dynamics_;
    }
  }

 private:
  static std::vector<LocationDuration> GetLocationDurations(
      const AgentProto& agent, const PopulationProfile& profile) {
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
    return durations;
  }

  static int64 GetLocationUuidForTypeOrDie(const AgentProto& agent,
                                           const LocationReference::Type type) {
    for (const auto& location : agent.locations()) {
      if (location.type() == type) {
        return location.uuid();
      }
    }
    LOG(FATAL) << "Location not found for type: " << type;
  }

  static VisitLocationDynamics GenerateVisitDynamics(
      PopulationProfileData& profile) {
    absl::BitGenRef gen = GetBitGen();
    return {
        .random_location_edges = profile.random_edges_distribution(gen),
    };
  }

  const std::unique_ptr<DurationSpecifiedVisitGenerator> generator_;
  const float susceptibility_;
  const VisitLocationDynamics visit_dynamics_;
};

const VisitGenerator& GetVisitGenerator(
    const AgentProto& agent, PopulationProfileData& profile,
    absl::flat_hash_map<std::string, std::unique_ptr<VisitGenerator>>& cache) {
  // Agents with the same set of locations and the same profile can share
  // a visit generator.
  std::string key = absl::StrCat(agent.population_profile_id());
  for (const LocationReference& ref : agent.locations()) {
    absl::StrAppend(&key, ",", ref.uuid());
  }
  std::unique_ptr<VisitGenerator>& value = cache[key];
  if (value == nullptr) {
    value = absl::make_unique<RiskLearningVisitGenerator>(agent, profile);
  }
  return *value;
}

}  // namespace

class RiskLearningSimulation : public Simulation {
 public:
  void Step(int steps, absl::Duration step_duration) override {
    current_changepoint_ = (stepwise_params_.size() > current_step_)
                               ? stepwise_params_[current_step_].changepoint()
                               : 1.0f;
    current_mobility_glm_scale_factor_ =
        (stepwise_params_.size() > current_step_)
            ? stepwise_params_[current_step_].mobility_glm_scale_factor()
            : 1.0f;
    UpdateCurrentLockdownMultipliers();
    sim_->Step(steps, step_duration);
    current_step_++;
  }
  void AddObserverFactory(ObserverFactoryBase* factory) override {
    sim_->AddObserverFactory(factory);
  }
  void RemoveObserverFactory(ObserverFactoryBase* factory) override {
    sim_->RemoveObserverFactory(factory);
  }
  static absl::StatusOr<std::unique_ptr<Simulation>> Build(
      const RiskLearningSimulationConfig& config, int num_workers) {
    std::vector<StepwiseParams> stepwise_params;
    stepwise_params.reserve(config.seeding_date_delta_days() +
                            config.stepwise_params_size());
    for (int i = 0; i < config.seeding_date_delta_days(); ++i) {
      stepwise_params.emplace_back();
      auto& entry = stepwise_params.back();
      entry.mutable_lockdown_state()->set_lockdown_status(
          LockdownStateProto::OFF);
      entry.mutable_lockdown_state()->set_lockdown_elderly_status(
          LockdownStateProto::OFF);
      entry.set_changepoint(1.0);
      entry.set_mobility_glm_scale_factor(1.0);
    }
    stepwise_params.insert(stepwise_params.end(),
                           config.stepwise_params().begin(),
                           config.stepwise_params().end());
    auto result =
        absl::WrapUnique(new RiskLearningSimulation(config, stepwise_params));

    // Home transmissibility is impacted by current lockdown multiplier.
    std::function<float()> home_transmissibility = [sim = result.get(),
                                                    &config]() -> float {
      return 1.0f *
             sim->current_lockdown_multipliers_[GraphLocation::HOUSEHOLD] *
             config.relative_transmission_home();
    };
    // Work and random network transmissibility is impacted by changepoint.
    std::function<float()> work_transmissibility = [sim = result.get(),
                                                    &config]() -> float {
      return config.relative_transmission_occupation() *
             sim->current_changepoint_;
    };
    std::function<float()> random_transmissibility = [sim = result.get(),
                                                      &config]() -> float {
      return config.relative_transmission_random() * sim->current_changepoint_;
    };
    // Work interaction drop prob. depends on the current lockdown multiplier
    // multiplied by the mobility glm scale factor.
    std::function<float(const GraphLocation::Type)> work_interaction_drop_prob =
        [sim = result.get(), &config](const GraphLocation::Type type) -> float {
      return 1.0 - config.daily_fraction_work() *
                       sim->current_lockdown_multipliers_[type] *
                       sim->current_mobility_glm_scale_factor_;
    };
    // Random interaction depends on lockdown multiplier and mobility glm scale
    // factor.
    std::function<float()> random_interaction_multiplier =
        [sim = result.get()]() -> float {
      return sim->current_lockdown_multipliers_[GraphLocation::RANDOM] *
             sim->current_mobility_glm_scale_factor_;
    };
    std::function<float()> non_work_drop_prob = []() -> float { return 0.0f; };

    TripleExposureGeneratorBuilder seg_builder(config.proximity_config());
    result->exposure_generator_ = seg_builder.Build();

    auto executor = NewExecutor(absl::GetFlag(FLAGS_num_reader_threads));
    auto exec = executor->NewExecution();
    absl::Mutex status_mu;
    absl::Mutex location_mu;
    // Read in locations.
    std::vector<std::unique_ptr<Location>> locations;
    int i = 0;
    std::vector<absl::Status> statuses;
    for (const std::string& location_file : config.location_file()) {
      exec->Add([&location_file, &locations, &home_transmissibility,
                 &work_transmissibility, &random_transmissibility,
                 &work_interaction_drop_prob, &non_work_drop_prob, &result,
                 &random_interaction_multiplier, &i, &location_mu, &status_mu,
                 &statuses]() {
        auto reader = MakeRecordReader(location_file);
        LocationProto proto;
        while (reader.ReadRecord(proto)) {
          {
            absl::MutexLock l(&location_mu);
            result->location_types_[proto.reference().uuid()] =
                proto.reference().type();
          }
          std::function<float()> transmissibility;
          if (proto.reference().type() == LocationReference::HOUSEHOLD) {
            transmissibility = home_transmissibility;
          } else if (proto.reference().type() == LocationReference::BUSINESS) {
            transmissibility = work_transmissibility;
          } else if (proto.reference().type() == LocationReference::RANDOM) {
            transmissibility = random_transmissibility;
          } else {
            absl::MutexLock l(&status_mu);
            statuses.push_back(absl::InvalidArgumentError(absl::StrCat(
                "Invalid type ", i, ": ", proto.reference().DebugString())));
            return;
          }
          switch (proto.location_case()) {
            case LocationProto::kGraph: {
              std::vector<std::pair<int64, int64>> edges;
              edges.reserve(proto.graph().edges_size());
              for (const GraphLocation::Edge& edge : proto.graph().edges()) {
                edges.push_back({edge.uuid_a(), edge.uuid_b()});
              }
              std::function<float()> drop_prob =
                  proto.reference().type() == LocationReference::BUSINESS
                      ? absl::bind_front(work_interaction_drop_prob,
                                         proto.graph().type())
                      : non_work_drop_prob;
              {
                absl::MutexLock l(&location_mu);
                locations.push_back(NewGraphLocation(
                    proto.reference().uuid(), transmissibility, drop_prob,
                    std::move(edges), *result->exposure_generator_));
              }
              break;
            }
            case LocationProto::kRandom: {
              absl::MutexLock l(&location_mu);
              locations.push_back(NewRandomGraphLocation(
                  proto.reference().uuid(), transmissibility,
                  random_interaction_multiplier, *result->exposure_generator_));
            } break;
            default: {
              absl::MutexLock l(&status_mu);
              statuses.push_back(absl::InvalidArgumentError(absl::StrCat(
                  "Invalid location ", i, ": ", proto.DebugString())));
            }
              return;
          }
          {
            absl::MutexLock l(&location_mu);
            i++;
          }
        }
        absl::Status status = reader.status();
        if (!status.ok()) {
          absl::MutexLock l(&status_mu);
          statuses.push_back(status);
          return;
        }
        reader.Close();
        LOG(INFO) << "Finished reading location_file: " << location_file;
      });
    }

    // TODO: Specify parameters explicitly here.
    result->transmission_model_ = absl::make_unique<HazardTransmissionModel>();

    result->infectivity_model_ =
        absl::make_unique<RiskLearningInfectivityModel>(
            config.global_profile());

    // Read in population profiles.
    absl::flat_hash_map<int, PopulationProfileData> profile_data;
    for (const PopulationProfile& profile : config.profiles()) {
      float mean = profile.random_visit_params().mean();
      float sd = profile.random_visit_params().stddev();
      float p = mean / sd / sd;
      int k = static_cast<int>((mean * mean / (sd * sd - mean)) + 0.5);
      profile_data[profile.id()] = {
          .profile = &profile,
          .random_edges_distribution =
              std::negative_binomial_distribution<int>(k, p)};
    }

    // Read in risk model config.
    auto risk_score_model_or =
        CreateLearningRiskScoreModel(config.risk_score_model());
    if (!risk_score_model_or.ok()) return risk_score_model_or.status();
    result->risk_score_model_ = risk_score_model_or.value();

    // Read in agents.
    absl::Mutex agent_mu;
    std::vector<std::unique_ptr<Agent>> agents;
    for (const std::string& agent_file : config.agent_file()) {
      exec->Add([&agent_file, &config, &result, &profile_data, &agents,
                 &agent_mu, &status_mu, &statuses]() {
        auto reader = MakeRecordReader(agent_file);
        AgentProto proto;
        while (reader.ReadRecord(proto)) {
          auto profile_iter = profile_data.find(proto.population_profile_id());
          if (profile_iter == profile_data.end()) {
            absl::MutexLock l(&status_mu);
            statuses.push_back(absl::InvalidArgumentError(
                absl::StrCat("Invalid population profile id for agent: ",
                             proto.DebugString())));
            return;
          }
          PopulationProfileData& agent_profile = profile_iter->second;
          // TODO: It is wasteful that we are making a new transition
          // model for each agent.  To fix this we need to make
          // GetNextHealthTransition thread safe.  This is complicated by the
          // fact that absl::discrete_distribution::operator() is non-const.
          auto risk_score_or = CreateLearningRiskScore(
              config.tracing_policy(), result->risk_score_model_,
              result->get_location_type_);
          if (!risk_score_or.ok()) {
            absl::MutexLock l(&status_mu);
            statuses.push_back(risk_score_or.status());
            return;
          }
          auto risk_score = CreateAppEnabledRiskScore(
              absl::Bernoulli(GetBitGen(),
                              agent_profile.profile->app_users_fraction()),
              std::move(*risk_score_or));
          {
            absl::MutexLock l(&agent_mu);
            agents.push_back(SEIRAgent::CreateSusceptible(
                proto.uuid(), result->transmission_model_.get(),
                result->infectivity_model_.get(),
                PTTSTransitionModel::CreateFromProto(
                    agent_profile.profile->transition_model()),
                GetVisitGenerator(proto, agent_profile,
                                  result->visit_gen_cache_),
                std::move(risk_score)));
          }
        }
        absl::Status status = reader.status();
        if (!status.ok()) {
          absl::MutexLock l(&status_mu);
          statuses.push_back(status);
        }
        reader.Close();
        LOG(INFO) << "Finished reading agent_file: " << agent_file;
      });
    }
    exec->Wait();
    if (!statuses.empty()) {
      // Arbitrarily return the first status.
      return statuses[0];
    }
    auto time_or = DecodeGoogleApiProto(config.init_time());
    if (!time_or.ok()) return time_or.status();
    const auto init_time = time_or.value();

    // Sample initial infections.
    if (config.n_seed_infections() > agents.size()) {
      return absl::InvalidArgumentError(
          "The number of seed infections is larger than the number of agents.");
    }
    int infected = 0;
    absl::BitGenRef gen = GetBitGen();
    while (infected < config.n_seed_infections()) {
      size_t idx = absl::Uniform<size_t>(gen, 0, agents.size());
      SEIRAgent* agent = static_cast<SEIRAgent*>(agents[idx].get());
      if (agent->NextHealthTransition().health_state !=
          HealthState::SUSCEPTIBLE) {
        continue;
      }
      infected++;
      agent->SeedInfection(init_time);
    }

    result->sim_ = num_workers > 1
                       ? ParallelSimulation(init_time, std::move(agents),
                                            std::move(locations), num_workers)
                       : SerialSimulation(init_time, std::move(agents),
                                          std::move(locations));
    result->sim_->AddObserverFactory(&result->summary_observer_);
    result->sim_->AddObserverFactory(&result->learning_observer_);
    return std::move(result);
  }

 private:
  RiskLearningSimulation(const RiskLearningSimulationConfig& config,
                         const std::vector<StepwiseParams>& stepwise_params)
      : config_(config),
        stepwise_params_(stepwise_params),
        get_location_type_(
            [this](int64 uuid) { return location_types_[uuid]; }),
        summary_observer_(config.summary_filename()),
        learning_observer_(config.learning_filename()) {
    current_lockdown_multipliers_.fill(1.0f);
  }

  static VisitLocationDynamics GenerateVisitDynamics(
      PopulationProfileData& profile) {
    absl::BitGenRef gen = GetBitGen();
    return {
        .random_location_edges = profile.random_edges_distribution(gen),
    };
  }
  void UpdateCurrentLockdownMultipliers() {
    const LockdownStateProto& lockdown_state =
        (stepwise_params_.size() > current_step_)
            ? stepwise_params_[current_step_].lockdown_state()
            : LockdownStateProto();

    for (const auto& lockdown_multiplier : config_.lockdown_multipliers()) {
      if (lockdown_multiplier.type() == GraphLocation::OCCUPATION_RETIRED ||
          lockdown_multiplier.type() == GraphLocation::OCCUPATION_ELDERLY) {
        current_lockdown_multipliers_[lockdown_multiplier.type()] =
            lockdown_state.lockdown_elderly_status() == LockdownStateProto::ON
                ? lockdown_multiplier.multiplier()
                : 1.0f;
      } else {
        current_lockdown_multipliers_[lockdown_multiplier.type()] =
            lockdown_state.lockdown_status() == LockdownStateProto::ON
                ? lockdown_multiplier.multiplier()
                : 1.0f;
      }
    }
  }
  const RiskLearningSimulationConfig config_;
  const std::vector<StepwiseParams> stepwise_params_;
  std::unique_ptr<ExposureGenerator> exposure_generator_;
  std::unique_ptr<HazardTransmissionModel> transmission_model_;
  std::unique_ptr<RiskLearningInfectivityModel> infectivity_model_;
  LearningRiskScoreModel risk_score_model_;
  absl::flat_hash_map<int64, LocationReference::Type> location_types_;
  const LocationTypeFn get_location_type_;
  // location_types is filled during location loading in the constructor and is
  // const after that.
  // This is important as it may be accessed by multiple threads during the
  // run of the simulation.
  absl::flat_hash_map<std::string, std::unique_ptr<VisitGenerator>>
      visit_gen_cache_;
  SummaryObserverFactory summary_observer_;
  LearningObserverFactory learning_observer_;
  std::unique_ptr<Simulation> sim_;
  int current_step_ = 0;
  float current_changepoint_ = 1.0f;
  float current_mobility_glm_scale_factor_ = 1.0f;
  EnumIndexedArray<float, GraphLocation::Type, GraphLocation::Type_ARRAYSIZE>
      current_lockdown_multipliers_;
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
