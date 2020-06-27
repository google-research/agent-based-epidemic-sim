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

#include "agent_based_epidemic_sim/core/uuid_generator.h"

#include <atomic>

namespace pandemic {

int64 ShardedGlobalIdUuidGenerator::GenerateUuid() const {
  static std::atomic<uint32> local_id = 0;
  return static_cast<int64>(uuid_shard_) << 48 | (local_id++);
}

}  // namespace pandemic
