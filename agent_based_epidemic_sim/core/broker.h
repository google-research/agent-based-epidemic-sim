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

#ifndef AGENT_BASED_EPIDEMIC_SIM_CORE_BROKER_H_
#define AGENT_BASED_EPIDEMIC_SIM_CORE_BROKER_H_

#include "absl/synchronization/mutex.h"
#include "absl/types/span.h"
#include "agent_based_epidemic_sim/core/event.h"

namespace abesim {

// The Broker interface allows Agents and Locations to send messages to one
// another.
template <typename Msg>
class Broker {
 public:
  // Send a set of messages.
  virtual void Send(absl::Span<const Msg> msgs) = 0;

  virtual ~Broker() = default;
};

// BufferingBroker buffers up buffer_size messages before forwarding them to a
// receiver. Users may call Flush to flush messages earlier.
template <typename Msg>
class BufferingBroker : public Broker<Msg> {
 public:
  explicit BufferingBroker(const int buffer_size, Broker<Msg>* const receiver)
      : buffer_size_(buffer_size), receiver_(receiver) {}

  void Send(const absl::Span<const Msg> msgs) override {
    buffer_.insert(buffer_.end(), msgs.begin(), msgs.end());
    if (buffer_.size() >= buffer_size_) Flush();
  }

  void Flush() {
    receiver_->Send(buffer_);
    buffer_.clear();
  }

 private:
  const int buffer_size_;
  std::vector<Msg> buffer_;
  Broker<Msg>* receiver_;
};

}  // namespace abesim

#endif  // AGENT_BASED_EPIDEMIC_SIM_CORE_BROKER_H_
