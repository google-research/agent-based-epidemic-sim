#ifndef AGENT_BASED_EPIDEMIC_SIM_CORE_CONSTANTS_H_
#define AGENT_BASED_EPIDEMIC_SIM_CORE_CONSTANTS_H_

#include <array>

#include "agent_based_epidemic_sim/core/integral_types.h"

namespace abesim {
inline const uint8 kMaxDaysAfterInfection = 14;

// This table maps days since infection to an infectivity factor. It is computed
// using a Gamma CDF distribution and the parameters are defined in
// (broken link).
inline const std::array<float, kMaxDaysAfterInfection + 1> kInfectivityArray = {
    0.9706490058950823,    1.7351078930710577,    1.2019044766404108,
    0.6227475415319733,    0.2804915619270876,    0.1163222813140352,
    0.04568182435881363,   0.017259874839000156,  0.006335824965724157,
    0.002274361283270409,  0.0008019921659041529, 0.00027871327592632333,
    0.0000956941785834608, 0.0000325214311858168, 0};

}  // namespace abesim

#endif  // AGENT_BASED_EPIDEMIC_SIM_CORE_CONSTANTS_H_
