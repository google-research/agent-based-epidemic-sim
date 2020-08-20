#ifndef AGENT_BASED_EPIDEMIC_SIM_CORE_CONSTANTS_H_
#define AGENT_BASED_EPIDEMIC_SIM_CORE_CONSTANTS_H_

#include <array>

#include "agent_based_epidemic_sim/core/integral_types.h"

namespace abesim {
// Maximum number of proximity recordings for a given proximity trace. Each
// recording reflects the distance between two hosts at a fixed interval from
// the last recording. This effectively caps the maximum length of a given
// exposure. If a longer exposure takes place, multiple Exposure objects should
// be created.
inline constexpr uint8 kMaxTraceLength = 20;

// Number of days after initial infection that the host is considered to still
// be infectious.
inline constexpr uint8 kMaxDaysAfterInfection = 14;

// This table maps days since infection to an infectivity factor. It is computed
// using a Gamma CDF distribution and the parameters are defined in
// (broken link).
inline const std::array<float, kMaxDaysAfterInfection + 1> kInfectivityArray = {
    0,
    0.0005190573909275743,
    0.01789205231287749,
    0.08197704117195806,
    0.15942404868622712,
    0.19639311438282891,
    0.18291684433819838,
    0.1411785741577649,
    0.095234136299998,
    0.05806124055356765,
    0.03271863716715262,
    0.017312313915856636,
    0.008700469912807929,
    0.004188825154643472,
    0};

}  // namespace abesim

#endif  // AGENT_BASED_EPIDEMIC_SIM_CORE_CONSTANTS_H_
