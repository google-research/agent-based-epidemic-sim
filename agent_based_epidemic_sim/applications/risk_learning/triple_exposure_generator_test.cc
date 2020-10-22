#include "agent_based_epidemic_sim/applications/risk_learning/triple_exposure_generator.h"

#include <memory>

#include "absl/time/time.h"
#include "agent_based_epidemic_sim/applications/risk_learning/config.pb.h"
#include "agent_based_epidemic_sim/applications/risk_learning/triple_exposure_generator_builder.h"
#include "agent_based_epidemic_sim/core/exposure_generator.h"
#include "agent_based_epidemic_sim/core/parse_text_proto.h"
#include "gmock/gmock.h"
#include "gtest/gtest.h"

namespace abesim {
namespace {

const Visit kInfectiousVisit = {.infectivity = 1.0, .symptom_factor = 1.0};
const Visit kSusceptibleVisit = {.infectivity = 0.0, .symptom_factor = 0.0};

TEST(TripleExposureGeneratorTest, CorrectOrderingOfExposures) {
  TripleExposureGenerator generator;
  ExposurePair exposures =
      generator.Generate(1.0, kInfectiousVisit, kSusceptibleVisit);
  EXPECT_EQ(exposures.host_a.infectivity, kSusceptibleVisit.infectivity);
  EXPECT_EQ(exposures.host_b.infectivity, kInfectiousVisit.infectivity);

  EXPECT_EQ(exposures.host_a.symptom_factor, kSusceptibleVisit.symptom_factor);
  EXPECT_EQ(exposures.host_b.symptom_factor, kInfectiousVisit.symptom_factor);
}

TEST(TripleExposureGeneratorTest, CorrectDataIsMirrored) {
  TripleExposureGenerator generator;
  ExposurePair exposures =
      generator.Generate(1.0, kInfectiousVisit, kSusceptibleVisit);
  EXPECT_EQ(exposures.host_a.start_time, exposures.host_b.start_time);
  EXPECT_EQ(exposures.host_a.duration, exposures.host_b.duration);
  EXPECT_EQ(exposures.host_a.distance, exposures.host_b.distance);
  EXPECT_EQ(exposures.host_a.attenuation, exposures.host_b.attenuation);
}

}  // namespace
}  // namespace abesim
