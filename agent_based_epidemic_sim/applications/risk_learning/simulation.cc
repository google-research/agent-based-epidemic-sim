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
#include "agent_based_epidemic_sim/applications/risk_learning/hazard_transmission_model.h"
#include "agent_based_epidemic_sim/applications/risk_learning/infectivity_model.h"
#include "agent_based_epidemic_sim/applications/risk_learning/observers.h"
#include "agent_based_epidemic_sim/applications/risk_learning/risk_score.h"
#include "agent_based_epidemic_sim/applications/risk_learning/triple_exposure_generator.h"
#include "agent_based_epidemic_sim/applications/risk_learning/triple_exposure_generator_builder.h"
#include "agent_based_epidemic_sim/core/agent.h"
#include "agent_based_epidemic_sim/core/duration_specified_visit_generator.h"
#include "agent_based_epidemic_sim/core/enum_indexed_array.h"
#include "agent_based_epidemic_sim/core/event.h"
#include "agent_based_epidemic_sim/core/exposure_generator.h"
#include "agent_based_epidemic_sim/core/graph_location.h"
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
ABSL_FLAG(bool, disable_learning_observer, false,
          "If true, disable writing learning outputs.");
ABSL_FLAG(int, max_population, -1, "If nonnegative, the max number of agents.");

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

const ProximityConfigProto* DefaultProximityConfig(
    const RiskLearningSimulationConfig& config) {
  // Returns a pointer on the proximity config proto to be used as default.
  // Returns a nullptr if no default is provided.
  if (config.has_specific_proximity_config()) {
    const auto& pc_map = config.specific_proximity_config().proximity_config();
    const auto& iter = pc_map.find(LocationReference::UNKNOWN);
    if (iter != pc_map.end()) {
      return &iter->second;
    }
  }
  if (config.has_proximity_config()) {
    return &config.proximity_config();
  }
  return nullptr;
}

bool ValidSpecificProximityConfig(const RiskLearningSimulationConfig& config) {
  // Returns false if an invalid specific proximity config is provided.
  if (!config.has_specific_proximity_config()) return true;
  // Checking validity of map keys.
  for (const auto& kv : config.specific_proximity_config().proximity_config()) {
    if (!LocationReference::Type_IsValid(kv.first)) {
      return false;
    }
  }
  return true;
}

}  // namespace

class RiskLearningSimulation : public Simulation {
 public:
  void Step(int steps, absl::Duration step_duration) override {
    for (int i = 0; i < steps; ++i) {
      LockdownStateProto lockdown_state;
      if (stepwise_params_.empty()) {
        current_changepoint_ = 1.0f;
        current_mobility_glm_scale_factor_ = 1.0f;
      } else {
        const int stepwise_params_index =
            stepwise_params_.size() > current_step_
                ? current_step_
                : stepwise_params_.size() - 1;
        current_changepoint_ =
            stepwise_params_[stepwise_params_index].changepoint();
        current_mobility_glm_scale_factor_ =
            stepwise_params_[stepwise_params_index].mobility_glm_scale_factor();
        lockdown_state =
            stepwise_params_[stepwise_params_index].lockdown_state();
      }
      UpdateCurrentLockdownMultipliers(lockdown_state);
      VLOG(1) << "Current time-varying parameter values: \n"
              << "current_changepoint_: " << current_changepoint_
              << " current_mobility_glm_scale_factor_: "
              << current_mobility_glm_scale_factor_;
      VLOG(1) << "Current lockdown multipliers:";
      for (int i = 0; i < GraphLocation::Type_ARRAYSIZE; ++i) {
        VLOG(1) << "Type " << GraphLocation::Type_Name(i) << ": "
                << current_lockdown_multipliers_[GraphLocation::Type(i)];
      }
      UpdateCurrentRiskScoreModel(init_time_ + step_duration * current_step_);
      sim_->Step(/*steps=*/1, step_duration);
      current_step_++;
    }
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
    auto result = absl::WrapUnique(
        new RiskLearningSimulation(config, stepwise_params, num_workers));

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

    // Setting up result->exposure_generators_ based on proximity configs.
    if (!ValidSpecificProximityConfig(config)) {
      return absl::InvalidArgumentError(
          "Invalid key in map "
          "config.specific_proximity_config.proximity_config .");
    }
    const ProximityConfigProto* default_pc = DefaultProximityConfig(config);
    for (int iloc = 0; iloc < LocationReference::Type_ARRAYSIZE; ++iloc) {
      if (!LocationReference::Type_IsValid(iloc) ||
          LocationReference::Type_Name(iloc) == "UNKNOWN")
        continue;
      if (config.has_specific_proximity_config()) {
        const auto& pc_map =
            config.specific_proximity_config().proximity_config();
        const auto iter = pc_map.find(iloc);
        if (iter != pc_map.end()) {
          TripleExposureGeneratorBuilder seg_builder(iter->second);
          result->exposure_generators_[LocationReference::Type(iloc)] =
              seg_builder.Build();
          continue;
        }
      }
      // No specific proximity config has been provided for location iloc.
      // Use the default if any or else an empty proximity config.
      if (default_pc != nullptr) {
        TripleExposureGeneratorBuilder seg_builder(*default_pc);
        result->exposure_generators_[LocationReference::Type(iloc)] =
            seg_builder.Build();
      } else {
        TripleExposureGeneratorBuilder seg_builder(
            ProximityConfigProto::default_instance());
        result->exposure_generators_[LocationReference::Type(iloc)] =
            seg_builder.Build();
      }
    }

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
                const int64 uuid = proto.reference().uuid();
                absl::MutexLock l(&location_mu);
                locations.push_back(NewGraphLocation(
                    uuid, transmissibility, drop_prob, std::move(edges),
                    *result->exposure_generators_[result->get_location_type_(
                        uuid)]));
              }
              break;
            }
            case LocationProto::kRandom: {
              const int64 uuid = proto.reference().uuid();
              absl::MutexLock l(&location_mu);
              locations.push_back(NewRandomGraphLocation(
                  uuid, transmissibility, random_interaction_multiplier,
                  *result->exposure_generators_[result->get_location_type_(
                      uuid)]));
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

    auto time_or = DecodeGoogleApiProto(config.init_time());
    if (!time_or.ok()) return time_or.status();
    result->init_time_ = time_or.value();

    if (!config.has_risk_score_config()) {
      return absl::InvalidArgumentError("No risk score config found.");
    }

    if (!config.risk_score_config().has_model_proto() &&
        config.risk_score_config().timestamped_model().empty()) {
      return absl::InvalidArgumentError(
          "No risk score model config found in risk score config.");
    }

    // Read in risk score model config.
    if (!config.risk_score_config().timestamped_model().empty()) {
      for (const auto& timestamped_model :
           config.risk_score_config().timestamped_model()) {
        auto time_or = DecodeGoogleApiProto(timestamped_model.start_time());
        if (!time_or.ok()) return time_or.status();
        const auto start_time = *time_or;
        auto risk_score_model_or =
            CreateLearningRiskScoreModel(timestamped_model.model_proto());
        if (!risk_score_model_or.ok()) return risk_score_model_or.status();
        result->risk_score_models_.push_back(
            {start_time, std::move(*risk_score_model_or)});
      }
    } else {
      auto risk_score_model_or = CreateLearningRiskScoreModel(
          config.risk_score_config().model_proto());
      if (!risk_score_model_or.ok()) return risk_score_model_or.status();
      result->risk_score_models_.push_back(
          {result->init_time_, std::move(*risk_score_model_or)});
    }

    std::function<const RiskScoreModel* const()> get_model_fn =
        [sim = result.get()]() -> RiskScoreModel* {
      return sim->current_risk_score_model_;
    };

    result->risk_score_model_ =
        CreateTimeVaryingRiskScoreModel(std::move(get_model_fn));
    // Read in risk score policy config if set. Otherwise fallback to default
    // values set in struct.
    if (config.risk_score_config().has_policy_proto()) {
      auto risk_score_policy_or = CreateLearningRiskScorePolicy(
          config.risk_score_config().policy_proto());
      if (!risk_score_policy_or.ok()) return risk_score_policy_or.status();
      result->risk_score_policy_ = *risk_score_policy_or;
    }

    // Read in agents.
    absl::Mutex agent_mu;
    std::vector<std::unique_ptr<Agent>> agents;
    const int max_population = absl::GetFlag(FLAGS_max_population);
    for (const std::string& agent_file : config.agent_file()) {
      exec->Add([&agent_file, &config, &result, &profile_data, &agents,
                 &agent_mu, &status_mu, &statuses, &max_population]() {
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
          // TODO: Use a Builder factory instead. It is wasteful to
          // create a TracingPolicy every time.
          auto risk_score_or = CreateLearningRiskScore(
              config.tracing_policy(), result->risk_score_policy_,
              result->risk_score_model_.get(), result->get_location_type_);
          if (!risk_score_or.ok()) {
            absl::MutexLock l(&status_mu);
            statuses.push_back(risk_score_or.status());
            return;
          }
          auto risk_score = CreateAppEnabledRiskScore(
              absl::Bernoulli(GetBitGen(),
                              agent_profile.profile->app_users_fraction()),
              std::move(*risk_score_or));
          TransmissionModel* transmission_model;
          if (config.append_hazard_to_test_results()) {
            auto hazard = absl::make_unique<Hazard>();
            transmission_model = hazard->GetTransmissionModel();
            risk_score = CreateHazardQueryingRiskScore(std::move(hazard),
                                                       std::move(risk_score));
          } else {
            transmission_model = result->transmission_model_.get();
          }
          {
            absl::MutexLock l(&agent_mu);
            if (max_population > 0 && agents.size() == max_population) {
              break;
            }
            // TODO: It is wasteful that we are making a new transition
            // model for each agent.  To fix this we need to make
            // GetNextHealthTransition thread safe.  This is complicated by the
            // fact that absl::discrete_distribution::operator() is non-const.
            agents.push_back(SEIRAgent::CreateSusceptible(
                proto.uuid(), transmission_model,
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
      agent->SeedInfection(result->init_time_);
    }

    result->sim_ =
        num_workers > 1
            ? ParallelSimulation(result->init_time_, std::move(agents),
                                 std::move(locations), num_workers)
            : SerialSimulation(result->init_time_, std::move(agents),
                               std::move(locations));
    result->sim_->AddObserverFactory(result->summary_observer_.get());
    if (ABSL_PREDICT_TRUE(absl::GetFlag(FLAGS_disable_learning_observer))) {
      LOG(WARNING) << "Learning outputs disabled.";
    } else if (nullptr == result->learning_observer_) {
      LOG(WARNING) << "No learning filename specified, not writing outputs.";
    } else {
      result->sim_->AddObserverFactory(result->learning_observer_.get());
    }
    if (nullptr == result->hazard_histogram_observer_) {
      LOG(WARNING)
          << "No hazard histogram filename specified, not writing outputs.";
    } else {
      result->sim_->AddObserverFactory(
          result->hazard_histogram_observer_.get());
    }
    return std::move(result);
  }

 private:
  RiskLearningSimulation(const RiskLearningSimulationConfig& config,
                         const std::vector<StepwiseParams>& stepwise_params,
                         const int num_workers)
      : config_(config),
        stepwise_params_(stepwise_params),
        get_location_type_(
            [this](int64 uuid) { return location_types_[uuid]; }),
        summary_observer_(absl::make_unique<SummaryObserverFactory>(
            config.summary_filename())) {
    if (!config.learning_filename().empty()) {
      learning_observer_ = absl::make_unique<LearningObserverFactory>(
          config.learning_filename(), num_workers);
    }
    if (!config.hazard_histogram_filename().empty()) {
      hazard_histogram_observer_ =
          absl::make_unique<HazardHistogramObserverFactory>(
              config.hazard_histogram_filename());
    }
    current_lockdown_multipliers_.fill(1.0f);
  }

  static VisitLocationDynamics GenerateVisitDynamics(
      PopulationProfileData& profile) {
    absl::BitGenRef gen = GetBitGen();
    return {
        .random_location_edges = profile.random_edges_distribution(gen),
    };
  }

  void UpdateCurrentLockdownMultipliers(
      const LockdownStateProto& lockdown_state) {
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

  void UpdateCurrentRiskScoreModel(const absl::Time current_time) {
    auto iter = std::upper_bound(
        risk_score_models_.begin(), risk_score_models_.end(), current_time,
        [](const absl::Time time,
           const std::pair<absl::Time, std::unique_ptr<RiskScoreModel>>&
               key_val) { return time < key_val.first; });
    CHECK(iter != risk_score_models_.begin());
    current_risk_score_model_ = (iter - 1)->second.get();
  }

  const RiskLearningSimulationConfig config_;
  const std::vector<StepwiseParams> stepwise_params_;
  EnumIndexedArray<std::unique_ptr<ExposureGenerator>, LocationReference::Type,
                   LocationReference::Type_ARRAYSIZE>
      exposure_generators_;
  std::unique_ptr<HazardTransmissionModel> transmission_model_;
  std::unique_ptr<RiskLearningInfectivityModel> infectivity_model_;
  std::unique_ptr<RiskScoreModel> risk_score_model_;
  std::vector<std::pair<absl::Time, std::unique_ptr<RiskScoreModel>>>
      risk_score_models_;
  LearningRiskScorePolicy risk_score_policy_;
  absl::flat_hash_map<int64, LocationReference::Type> location_types_;
  const LocationTypeFn get_location_type_;
  // location_types is filled during location loading in the constructor and is
  // const after that.
  // This is important as it may be accessed by multiple threads during the
  // run of the simulation.
  absl::flat_hash_map<std::string, std::unique_ptr<VisitGenerator>>
      visit_gen_cache_;
  std::unique_ptr<ObserverFactoryBase> summary_observer_;
  std::unique_ptr<ObserverFactoryBase> learning_observer_;
  std::unique_ptr<ObserverFactoryBase> hazard_histogram_observer_;
  std::unique_ptr<Simulation> sim_;
  absl::Time init_time_;
  int current_step_ = 0;
  float current_changepoint_ = 1.0f;
  float current_mobility_glm_scale_factor_ = 1.0f;
  // Owned by risk_score_models_.
  RiskScoreModel* current_risk_score_model_;
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
