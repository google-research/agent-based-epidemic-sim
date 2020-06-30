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

#ifndef THIRD_PARTY_AGENT_BASED_EPIDEMIC_SIM_CORE_UUID_GENERATOR_H_
#define THIRD_PARTY_AGENT_BASED_EPIDEMIC_SIM_CORE_UUID_GENERATOR_H_

#include "agent_based_epidemic_sim/core/integral_types.h"

namespace abesim {

class UuidGenerator {
 public:
  virtual int64 GenerateUuid() const = 0;
  virtual ~UuidGenerator() = default;
};

class ShardedGlobalIdUuidGenerator : public UuidGenerator {
 public:
  explicit ShardedGlobalIdUuidGenerator(int16 uuid_shard)
      : uuid_shard_(uuid_shard) {}
  int64 GenerateUuid() const override;

 private:
  const int16 uuid_shard_;
};
}  // namespace abesim

#endif  // THIRD_PARTY_AGENT_BASED_EPIDEMIC_SIM_CORE_UUID_GENERATOR_H_
