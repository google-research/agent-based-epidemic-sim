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

#include "absl/flags/flag.h"
#include "absl/strings/str_split.h"
#include "absl/time/time.h"
#include "agent_based_epidemic_sim/applications/home_work/config.pb.h"
#include "agent_based_epidemic_sim/applications/risk_learning/config.pb.h"
#include "agent_based_epidemic_sim/core/parse_text_proto.h"
#include "agent_based_epidemic_sim/core/risk_score.h"
#include "agent_based_epidemic_sim/port/file_utils.h"
#include "agent_based_epidemic_sim/port/status_matchers.h"
#include "gmock/gmock.h"
#include "gtest/gtest.h"

namespace abesim {
namespace {
constexpr char kConfigPath[] =
    "agent_based_epidemic_sim/applications/risk_learning/"
    "config.pbtxt";

constexpr char kExpectedHeader[] =
    "distancing_stage_start,distancing_stage_essential_workers,"
    "distancing_stage_start,distancing_stage_essential_workers,business_alpha,"
    "business_beta,household_size,household_size_probability,household_size,"
    "household_size_probability,household_size,household_size_probability,"
    "household_size,household_size_probability,household_size,"
    "household_size_probability,initial_prob_SUSCEPTIBLE,"
    "initial_prob_INFECTIOUS,top_business_size,top_business_size,"
    "top_business_size,top_business_size,top_business_size,timestep_end,agents,"
    "SUSCEPTIBLE,EXPOSED,INFECTIOUS,RECOVERED,ASYMPTOMATIC,"
    "PRE_SYMPTOMATIC_MILD,PRE_SYMPTOMATIC_SEVERE,SYMPTOMATIC_MILD,"
    "SYMPTOMATIC_SEVERE,SYMPTOMATIC_HOSPITALIZED,SYMPTOMATIC_CRITICAL,"
    "SYMPTOMATIC_HOSPITALIZED_RECOVERING,REMOVED,"
    "home_0,home_1h,home_2h,home_4h,home_8h,home_16h,"
    "work_0,work_1h,work_2h,work_4h,work_8h,work_16h,contact_1,contact_2,"
    "contact_4,contact_8,contact_16,contact_32,contact_64,contact_128,"
    "contact_256,contact_512";

constexpr int kExpectedContentsLength = 60;

TEST(SimulationTest, RunsSimulation) {
  const std::string config_path = absl::StrCat("./", "/", kConfigPath);
  std::string contents;
  PANDEMIC_ASSERT_OK(file::GetContents(config_path, &contents));
  ContactTracingHomeWorkSimulationConfig config =
      ParseTextProtoOrDie<ContactTracingHomeWorkSimulationConfig>(contents);
  config.mutable_home_work_config()->set_num_steps(1);
  const std::string output_file_path =
      absl::StrCat(getenv("TEST_TMPDIR"), "/", "output.csv");
  RunSimulation(output_file_path, "", config, /*num_workers=*/1);

  std::string output;
  PANDEMIC_ASSERT_OK(file::GetContents(output_file_path, &output));
  const std::vector<std::string> lines =
      absl::StrSplit(output, '\n', absl::SkipEmpty());
  EXPECT_EQ(kExpectedHeader, lines[0]);
  const std::vector<std::string> first_row =
      absl::StrSplit(lines[1], absl::ByString(","));
  EXPECT_EQ(kExpectedContentsLength, first_row.size());
}

}  // namespace
}  // namespace abesim
