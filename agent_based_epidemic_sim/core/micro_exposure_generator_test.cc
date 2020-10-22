#include "agent_based_epidemic_sim/core/micro_exposure_generator.h"

#include "agent_based_epidemic_sim/core/event.h"
#include "agent_based_epidemic_sim/core/exposure_generator.h"
#include "agent_based_epidemic_sim/core/micro_exposure_generator_builder.h"
#include "gmock/gmock.h"
#include "gtest/gtest.h"

namespace abesim {
namespace {

const Visit kInfectiousVisit = {.infectivity = 1.0, .symptom_factor = 1.0};
const Visit kSusceptibleVisit = {.infectivity = 0.0, .symptom_factor = 0.0};

const std::vector<std::vector<float>> kDistribution = {{1.0f}};

TEST(MicroExposureGeneratorTest, CorrectlyDrawsFromDistribution) {
  MicroExposureGeneratorBuilder meg_builder(kDistribution);
  std::unique_ptr<ExposureGenerator> generator = meg_builder.Build();

  ExposurePair exposures =
      generator->Generate(1.0, kInfectiousVisit, kSusceptibleVisit);

  ProximityTrace proximity_trace = exposures.host_a.proximity_trace;
  EXPECT_EQ(proximity_trace.values[0], 1.0f);
  for (int i = 1; i < proximity_trace.values.size(); ++i) {
    EXPECT_EQ(proximity_trace.values[i], std::numeric_limits<float>::max());
  }
}

TEST(MicroExposureGeneratorTest, ProximityTracesEqual) {
  MicroExposureGeneratorBuilder meg_builder(kDistribution);
  std::unique_ptr<ExposureGenerator> generator = meg_builder.Build();

  ExposurePair exposure_pair =
      generator->Generate(1.0, kInfectiousVisit, kSusceptibleVisit);

  EXPECT_EQ(exposure_pair.host_a.proximity_trace,
            exposure_pair.host_b.proximity_trace);
}

TEST(MicroExposureGeneratorTest, CorrectOrderingOfExposures) {
  MicroExposureGeneratorBuilder meg_builder(kDistribution);
  std::unique_ptr<ExposureGenerator> generator = meg_builder.Build();

  ExposurePair exposure_pair =
      generator->Generate(1.0, kInfectiousVisit, kSusceptibleVisit);

  EXPECT_EQ(exposure_pair.host_a.infectivity, kSusceptibleVisit.infectivity);
  EXPECT_EQ(exposure_pair.host_b.infectivity, kInfectiousVisit.infectivity);

  EXPECT_EQ(exposure_pair.host_a.symptom_factor,
            kSusceptibleVisit.symptom_factor);
  EXPECT_EQ(exposure_pair.host_b.symptom_factor,
            kInfectiousVisit.symptom_factor);
}

}  // namespace
}  // namespace abesim
