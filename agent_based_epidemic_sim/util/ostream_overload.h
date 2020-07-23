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

#ifndef AGENT_BASED_EPIDEMIC_SIM_UTIL_OSTREAM_OVERLOAD_H_
#define AGENT_BASED_EPIDEMIC_SIM_UTIL_OSTREAM_OVERLOAD_H_

#define OVERLOAD_VECTOR_OSTREAM_OPS                                     \
  template <typename T>                                                 \
  std::ostream& operator<<(std::ostream& os, const std::vector<T>& v) { \
    os << "[";                                                          \
    for (int i = 0; i < v.size(); i++) {                                \
      os << v[i];                                                       \
      if (i != v.size() - 1) os << ", ";                                \
    }                                                                   \
    os << "]";                                                          \
    return os;                                                          \
  }

#define OVERLOAD_ARRAY_OSTREAM_OPS                                        \
  template <typename T, size_t N>                                         \
  std::ostream& operator<<(std::ostream& os, const std::array<T, N>& a) { \
    os << "[";                                                            \
    for (int i = 0; i < a.size(); i++) {                                  \
      os << a[i];                                                         \
      if (i != a.size() - 1) os << ", ";                                  \
    }                                                                     \
    os << "]";                                                            \
    return os;                                                            \
  }

#endif  // AGENT_BASED_EPIDEMIC_SIM_UTIL_OSTREAM_OVERLOAD_H_
