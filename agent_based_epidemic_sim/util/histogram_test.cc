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

#include "agent_based_epidemic_sim/util/histogram.h"

#include "gtest/gtest.h"

namespace abesim {
namespace {
constexpr int kSize = 10;
constexpr float kScale = 0.1;
TEST(HistogramTest, ComputesLinearHistogram) {
  LinearHistogram<float, kSize> histogram;
  histogram.Add(1, kScale);
  histogram.Add(0.1, kScale);
  histogram.Add(0, kScale);
  histogram.Add(.95, kScale);
  histogram.Add(0.09, kScale);
  const std::string expected = ",2,1,0,0,0,0,0,0,0,2";
  std::string actual;
  histogram.AppendValuesToString(&actual);
  EXPECT_EQ(actual, expected);
}

}  // namespace
}  // namespace abesim
