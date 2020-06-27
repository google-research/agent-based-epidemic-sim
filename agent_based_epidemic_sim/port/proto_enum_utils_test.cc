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

#include "agent_based_epidemic_sim/port/proto_enum_utils.h"

#include "agent_based_epidemic_sim/port/testdata/enum_enumeration.pb.h"
#include "agent_based_epidemic_sim/port/testdata/enum_repeated_field_wrapper.pb.h"
#include "gmock/gmock.h"
#include "gtest/gtest.h"

namespace pandemic {
namespace {

using ::testing::ElementsAre;

TEST(EnumUtilsTest, EnumerateProtoEnum) {
  auto view = EnumerateEnumValues<EnumEnumerationMessage::Enum>();
  std::vector<EnumEnumerationMessage::Enum> all(view.begin(), view.end());
  EXPECT_THAT(all, ::testing::ElementsAre(EnumEnumerationMessage::A,
                                          EnumEnumerationMessage::B,
                                          EnumEnumerationMessage::D));
  all.clear();
  for (EnumEnumerationMessage::Enum e :
       EnumerateEnumValues<EnumEnumerationMessage::Enum>()) {
    all.push_back(e);
  }
  EXPECT_THAT(all,
              ElementsAre(EnumEnumerationMessage::A, EnumEnumerationMessage::B,
                          EnumEnumerationMessage::D));
  all.clear();
}

TEST(EnumUtilsTest, IteratesOverEnumRepeatedField) {
  EnumRepeatedFieldWrapperMessage message;
  message.add_enums(EnumRepeatedFieldWrapperMessage::A);
  message.add_enums(EnumRepeatedFieldWrapperMessage::B);
  message.add_enums(EnumRepeatedFieldWrapperMessage::C);
  message.add_enums(EnumRepeatedFieldWrapperMessage::D);
  message.add_enums(EnumRepeatedFieldWrapperMessage::E);
  message.add_enums(EnumRepeatedFieldWrapperMessage::A);

  std::vector<EnumRepeatedFieldWrapperMessage::Enum> all;
  for (auto e : REPEATED_ENUM_ADAPTER(message, enums)) {
    static_assert(
        std::is_same<decltype(e), EnumRepeatedFieldWrapperMessage::Enum>::value,
        "iterator of EnumRepeatedFieldWrapper should point to an "
        "EnumRepeatedFieldWrapperMessage::Enum");
    all.push_back(e);
  }

  EXPECT_THAT(all, ElementsAre(EnumRepeatedFieldWrapperMessage::A,
                               EnumRepeatedFieldWrapperMessage::B,
                               EnumRepeatedFieldWrapperMessage::C,
                               EnumRepeatedFieldWrapperMessage::D,
                               EnumRepeatedFieldWrapperMessage::E,
                               EnumRepeatedFieldWrapperMessage::A));
}

}  // namespace
}  // namespace pandemic
