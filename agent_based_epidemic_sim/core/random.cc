#include "agent_based_epidemic_sim/core/random.h"

#include "absl/random/random.h"

namespace abesim {

absl::BitGenRef GetBitGen() {
  thread_local absl::BitGen bitgen;
  return bitgen;
}

}  // namespace abesim
