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

#ifndef THIRD_PARTY_AGENT_BASED_EPIDEMIC_SIM_PORT_EXECUTOR_H_
#define THIRD_PARTY_AGENT_BASED_EPIDEMIC_SIM_PORT_EXECUTOR_H_

#include <functional>
#include <memory>

namespace abesim {

// An Execution allows for running functions in multiple threads.
class Execution {
 public:
  // Add a new function to the execution to be run in the near future,
  // potentially in another thread.
  virtual void Add(std::function<void()> fn) = 0;
  // Wait for all added functions to finish executing.  It is invalid to call
  // Add concurrently with or after calling Wait.
  virtual void Wait() = 0;

  virtual ~Execution() = default;
};

// An Executor can create new Executions.
class Executor {
 public:
  // Create a new execution.
  virtual std::unique_ptr<Execution> NewExecution() = 0;

  virtual ~Executor() = default;
};

std::unique_ptr<Executor> NewExecutor(int max_parallelism);

}  // namespace abesim

#endif  // THIRD_PARTY_AGENT_BASED_EPIDEMIC_SIM_PORT_EXECUTOR_H_
