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

#include "agent_based_epidemic_sim/core/enum_indexed_array.h"

#include "agent_based_epidemic_sim/core/integral_types.h"
#include "gtest/gtest.h"

namespace pandemic {
namespace {

enum class TestEnum {
  kCase0,
  kCase1,
  kCase2,
};

TEST(EnumIndexedArray, BracketOperator) {
  EnumIndexedArray<int64, TestEnum, 3> a{{1, 4, 6}};
  EXPECT_EQ(a[TestEnum::kCase0], 1);
  EXPECT_EQ(a[TestEnum::kCase1], 4);
  EXPECT_EQ(a[TestEnum::kCase2], 6);
  a[TestEnum::kCase1] = 3;
  EXPECT_EQ(a[TestEnum::kCase0], 1);
  EXPECT_EQ(a[TestEnum::kCase1], 3);
  EXPECT_EQ(a[TestEnum::kCase2], 6);
  a[TestEnum::kCase0] = 11;
  EXPECT_EQ(a[TestEnum::kCase0], 11);
  EXPECT_EQ(a[TestEnum::kCase1], 3);
  EXPECT_EQ(a[TestEnum::kCase2], 6);
  a[TestEnum::kCase2] = 15;
  EXPECT_EQ(a[TestEnum::kCase0], 11);
  EXPECT_EQ(a[TestEnum::kCase1], 3);
  EXPECT_EQ(a[TestEnum::kCase2], 15);
}

TEST(EnumIndexedArray, BracketOperatorConst) {
  const EnumIndexedArray<int64, TestEnum, 3> a{{2, 5, 8}};
  EXPECT_EQ(a[TestEnum::kCase0], 2);
  EXPECT_EQ(a[TestEnum::kCase1], 5);
  EXPECT_EQ(a[TestEnum::kCase2], 8);
}

}  // namespace
}  // namespace pandemic
