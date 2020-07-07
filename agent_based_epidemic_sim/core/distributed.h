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

#ifndef AGENT_BASED_EPIDEMIC_SIM_CORE_DISTRIBUTED_H_
#define AGENT_BASED_EPIDEMIC_SIM_CORE_DISTRIBUTED_H_

#include <functional>

#include "absl/types/span.h"
#include "agent_based_epidemic_sim/core/broker.h"

namespace abesim {

// A DistributedMessenger represents the bidirectional messages stream of a
// given type between the local node and all remote participants.  It acts as a
// Broker to accept messages, but also provides an interface to receive messages
// from remote nodes. It also has an interface to determine which messages
// should be kept locally or sent remotely.
template <typename Msg>
class DistributedMessenger : public Broker<Msg> {
 public:
  // This function returns true if the given message should be sent to a
  // remote node, false if the message should be processed locally.
  virtual bool IsMessageRemote(const Msg& msg) const = 0;

  // Supply a broker that should receive messages coming in from remote nodes.
  virtual void SetReceiveBrokerForNextPhase(Broker<Msg>* broker) = 0;

  // Wait for all messages from remote nodes to be sent to the ReceiveBroker
  // and finish sending all messages destined for remote nodes.
  virtual void FlushAndAwaitRemotes() = 0;
};

// A DistributedManager manages the communication infrastructure for interacting
// with remote nodes in a distributed simulation.
class DistributedManager {
 public:
  virtual DistributedMessenger<Visit>* VisitMessenger() = 0;
  virtual DistributedMessenger<ContactReport>* ContactReportMessenger() = 0;
  virtual DistributedMessenger<InfectionOutcome>* OutcomeMessenger() = 0;

  virtual ~DistributedManager() = default;
};

// DistributingBroker splits its incoming message stream, sending those destined
// for local processing to a local Broker and the rest to a
// DistributedMessenger.  To avoid sending many small batches to its targets,
// this broker also buffers data up to a given size. Users can call Flush to
// send buffered data early.
template <typename Msg>
class DistributingBroker : public Broker<Msg> {
 public:
  explicit DistributingBroker(const int buffer_size,
                              DistributedMessenger<Msg>* distributed_messenger,
                              Broker<Msg>* local_broker)
      : distributed_messenger_(distributed_messenger),
        buffering_remote_(buffer_size, distributed_messenger),
        buffering_local_(buffer_size, local_broker) {}

  void Send(absl::Span<const Msg> msgs) override {
    for (const Msg& msg : msgs) {
      if (distributed_messenger_->IsMessageRemote(msg)) {
        buffering_remote_.Send(absl::MakeConstSpan(&msg, 1));
      } else {
        buffering_local_.Send(absl::MakeConstSpan(&msg, 1));
      }
    }
  }

  void Flush() {
    buffering_local_.Flush();
    buffering_remote_.Flush();
  }

 private:
  const DistributedMessenger<Msg>* const distributed_messenger_;
  BufferingBroker<Msg> buffering_remote_;
  BufferingBroker<Msg> buffering_local_;
};

}  // namespace abesim

#endif  // AGENT_BASED_EPIDEMIC_SIM_CORE_DISTRIBUTED_H_
