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

#ifndef AGENT_BASED_EPIDEMIC_SIM_UTIL_HISTOGRAM_H_
#define AGENT_BASED_EPIDEMIC_SIM_UTIL_HISTOGRAM_H_

#include "absl/strings/str_format.h"

namespace abesim {
// Very simple histogram class.
template <typename T, int Size, typename Indexer>
class Histogram {
 public:
  Histogram() { buckets_.fill(0); }

  void Add(T value, T scale) {
    const size_t n = static_cast<size_t>(value / scale);
    const size_t index = indexer_(n);
    const size_t bucket = std::min(index, buckets_.size() - 1);
    buckets_[bucket]++;
  }

  void AppendValuesToString(std::string* dst) const {
    for (int bucket : buckets_) {
      absl::StrAppendFormat(dst, ",%d", bucket);
    }
  }

 private:
  Indexer indexer_;
  std::array<size_t, Size> buckets_;
};

struct linear_indexer {
  size_t operator()(size_t n) { return n; }
};

struct log2_indexer {
  size_t operator()(size_t n) {
    return n == 0 ? 0 : 1 + std::floor(std::log2(n));
  }
};

template <typename T, int Size>
using LinearHistogram = Histogram<T, Size, linear_indexer>;

template <typename T, int Size>
using Log2Histogram = Histogram<T, Size, log2_indexer>;

}  // namespace abesim

#endif  // AGENT_BASED_EPIDEMIC_SIM_UTIL_HISTOGRAM_H_
