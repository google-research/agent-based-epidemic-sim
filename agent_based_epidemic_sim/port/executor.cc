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

#include "agent_based_epidemic_sim/port/executor.h"

#include <thread>  // NOLINT: Open source only.
#include <vector>

#include "absl/memory/memory.h"
#include "absl/synchronization/mutex.h"

namespace abesim {
namespace {

class StdThreadExecutor : public Executor {
 public:
  explicit StdThreadExecutor(int workers);
  std::unique_ptr<Execution> NewExecution() override;

  ~StdThreadExecutor() override;

 private:
  absl::Mutex mu_;

  friend class StdThreadExecution;
  void Add(std::function<void()> fn) ABSL_LOCKS_EXCLUDED(mu_);
  bool Ready() ABSL_EXCLUSIVE_LOCKS_REQUIRED(mu_);
  std::vector<std::thread> threads_;
  bool done_ GUARDED_BY(mu_) = false;
  std::vector<std::function<void()> > work_ GUARDED_BY(mu_);
};

class StdThreadExecution : public Execution {
 public:
  explicit StdThreadExecution(StdThreadExecutor& executor)
      : executor_(executor) {}
  void Add(std::function<void()> fn) override {
    {
      absl::MutexLock l(&mu_);
      started_++;
    }
    executor_.Add([this, fn]() {
      fn();
      {
        absl::MutexLock l(&mu_);
        finished_++;
      }
    });
  }
  void Wait() override {
    mu_.LockWhen(absl::Condition(this, &StdThreadExecution::AllFinished));
    mu_.Unlock();
  }

 private:
  StdThreadExecutor& executor_;
  absl::Mutex mu_;
  bool AllFinished() ABSL_EXCLUSIVE_LOCKS_REQUIRED(mu_) {
    return started_ == finished_;
  }
  int started_ ABSL_GUARDED_BY(mu_) = 0;
  int finished_ ABSL_GUARDED_BY(mu_) = 0;
};

StdThreadExecutor::StdThreadExecutor(const int workers) {
  for (int i = 0; i < workers; ++i) {
    threads_.push_back(std::thread([this]() {
      while (true) {
        mu_.LockWhen(absl::Condition(this, &StdThreadExecutor::Ready));
        if (done_) {
          mu_.Unlock();
          return;
        }
        auto work = work_.back();
        work_.pop_back();
        mu_.Unlock();
        work();
      }
    }));
  }
}

bool StdThreadExecutor::Ready() { return done_ || !work_.empty(); }

StdThreadExecutor::~StdThreadExecutor() {
  {
    absl::MutexLock l(&mu_);
    done_ = true;
  }
  for (std::thread& thread : threads_) {
    thread.join();
  }
}

void StdThreadExecutor::Add(std::function<void()> fn) {
  absl::MutexLock l(&mu_);
  work_.push_back(fn);
}

std::unique_ptr<Execution> StdThreadExecutor::NewExecution() {
  return absl::make_unique<StdThreadExecution>(*this);
}

}  // namespace

std::unique_ptr<Executor> NewExecutor(int max_parallelism) {
  return absl::make_unique<StdThreadExecutor>(max_parallelism);
}

}  // namespace abesim
