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

#ifndef AGENT_BASED_EPIDEMIC_SIM_CORE_TIMESTEP_H_
#define AGENT_BASED_EPIDEMIC_SIM_CORE_TIMESTEP_H_

#include "absl/time/time.h"

namespace abesim {

// Timestep represents the simulation window for a single step.
class Timestep {
 public:
  Timestep(absl::Time start, absl::Duration duration)
      : start_(start), duration_(duration), end_(start + duration) {}

  absl::Time start_time() const { return start_; }
  absl::Time end_time() const { return end_; }
  absl::Duration duration() const { return duration_; }
  void Advance();

  bool operator==(const Timestep& other) const {
    return start_ == other.start_ && duration_ == other.duration_;
  }
  bool operator!=(const Timestep& other) const { return !(*this == other); }
  friend std::ostream& operator<<(std::ostream& strm,
                                  const Timestep& timestep) {
    return strm << "{" << timestep.start_time() << "," << timestep.end_time()
                << "," << timestep.duration() << "}";
  }

 private:
  absl::Time start_;
  absl::Duration duration_;
  absl::Time end_;
};

}  // namespace abesim

#endif  // AGENT_BASED_EPIDEMIC_SIM_CORE_TIMESTEP_H_
