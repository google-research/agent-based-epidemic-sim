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

#ifndef THIRD_PARTY_AGENT_BASED_EPIDEMIC_SIM_CORE_PARSE_TEXT_PROTO_H_
#define THIRD_PARTY_AGENT_BASED_EPIDEMIC_SIM_CORE_PARSE_TEXT_PROTO_H_

#include "agent_based_epidemic_sim/port/logging.h"
#include "google/protobuf/text_format.h"

namespace pandemic {

template <typename T>
T ParseTextProtoOrDie(const std::string& input) {
  T result;
  CHECK(google::protobuf::TextFormat::ParseFromString(input, &result));
  return result;
}

}  // namespace pandemic

#endif  // THIRD_PARTY_AGENT_BASED_EPIDEMIC_SIM_CORE_PARSE_TEXT_PROTO_H_