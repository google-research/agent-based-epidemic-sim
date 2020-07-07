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

#ifndef AGENT_BASED_EPIDEMIC_SIM_CORE_ENUM_INDEXED_ARRAY_H_
#define AGENT_BASED_EPIDEMIC_SIM_CORE_ENUM_INDEXED_ARRAY_H_

#include <array>

#include "absl/meta/type_traits.h"

namespace abesim {

// An std::array variant that can be indexed with enum class types.
template <typename T, typename Enum, size_t kSize>
struct EnumIndexedArray : public std::array<T, kSize> {
 public:
  using value_type = T;
  using enum_type = Enum;
  using array_type = std::array<value_type, kSize>;
  using reference = value_type&;
  using const_reference = const value_type&;

  constexpr reference operator[](enum_type enum_index) noexcept {
    return array_type::operator[](
        static_cast<absl::underlying_type_t<enum_type>>(enum_index));
  }

  constexpr const_reference operator[](enum_type enum_index) const noexcept {
    return array_type::operator[](
        static_cast<absl::underlying_type_t<enum_type>>(enum_index));
  }
};

}  // namespace abesim

#endif  // AGENT_BASED_EPIDEMIC_SIM_CORE_ENUM_INDEXED_ARRAY_H_
