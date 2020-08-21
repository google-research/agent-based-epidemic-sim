#ifndef AGENT_BASED_EPIDEMIC_SIM_CORE_RANDOM_H_
#define AGENT_BASED_EPIDEMIC_SIM_CORE_RANDOM_H_

#include "absl/random/bit_gen_ref.h"

namespace abesim {

// Get a BitGen that can be used on the current thread.  The returned
// value may not be passed to another thread.
absl::BitGenRef GetBitGen();

}  // namespace abesim

#endif  // AGENT_BASED_EPIDEMIC_SIM_CORE_RANDOM_H_
