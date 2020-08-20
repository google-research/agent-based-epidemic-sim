#ifndef AGENT_BASED_EPIDEMIC_SIM_CORE_CONSTANTS_H_
#define AGENT_BASED_EPIDEMIC_SIM_CORE_CONSTANTS_H_

#include <array>
#include <vector>

#include "absl/time/time.h"
#include "agent_based_epidemic_sim/core/integral_types.h"

namespace abesim {

// Default transmissibility of a location used when calculating transmission
// likelihood between two hosts at a given location.
inline constexpr float kLocationTransmissibility = 1;

// TODO: Move into the visit message about the visiting agent.
// Default susceptibility of a host used when calculating transmission
// likelihood between two hosts. Usually a function of the host's age.
inline constexpr float kSusceptibility = 1;

// Maximum number of proximity recordings for a given proximity trace. Each
// recording reflects the distance between two hosts at a fixed interval from
// the last recording. This effectively caps the maximum length of a given
// exposure. If a longer exposure takes place, multiple Exposure objects should
// be created.
inline constexpr uint8 kMaxTraceLength = 20;

// Number of days after initial infection that the host is considered to still
// be infectious.
inline constexpr uint8 kMaxDaysAfterInfection = 14;

// Represents a fixed time interval between recordings in a proximity trace.
inline const absl::Duration kProximityTraceInterval = absl::Minutes(5);

// TODO: Dummy data for now. Load this into memory from file.
inline const std::vector<std::vector<float>> kNonParametricTraceDistribution = {
    {9.06893969},
    {2.17069263, 1.34896927, 1.25818902},
    {0.82138789, 6.22004292},
    {2.05025381},
    {3.24773771},
    {4.47790356},
    {8.76846823},
    {2.46735085, 1.18301727, 2.31253068, 0.6587179},
    {5.40369733, 5.72922553, 4.00336149},
    {3.90483315, 1.13448638, 1.52669001, 4.54364751, 0.00735043, 1.91172425,
     0.29878502, 1.03358558, 3.7044072, 1.87280156, 1.3717239, 2.72108035,
     0.81446531, 3.54814224, 1.90291171, 0.38753284, 1.42542508}};

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
