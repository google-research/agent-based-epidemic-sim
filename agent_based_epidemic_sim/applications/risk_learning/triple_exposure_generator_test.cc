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

const Visit kInfectiousSymptomaticVisit = {
    .susceptibility = 0.0, .infectivity = 1.0, .symptom_factor = 0.7};
const Visit kSusceptibleVisit = {
    .susceptibility = 1.0, .infectivity = 0.0, .symptom_factor = 0.0};
constexpr float kLocationTransmissibility = 0.314;

TEST(TripleExposureGeneratorTest, CorrectOrderingOfExposures) {
  TripleExposureGenerator generator;
  ExposurePair exposure_pair =
      generator.Generate(kLocationTransmissibility, kInfectiousSymptomaticVisit,
                         kSusceptibleVisit);
  EXPECT_EQ(exposure_pair.host_a.infectivity, kSusceptibleVisit.infectivity);
  EXPECT_EQ(exposure_pair.host_b.infectivity,
            kInfectiousSymptomaticVisit.infectivity);
  EXPECT_EQ(exposure_pair.host_a.symptom_factor,
            kSusceptibleVisit.symptom_factor);
  EXPECT_EQ(exposure_pair.host_b.symptom_factor,
            kInfectiousSymptomaticVisit.symptom_factor);
  EXPECT_EQ(exposure_pair.host_a.susceptibility,
            kInfectiousSymptomaticVisit.susceptibility);
  EXPECT_EQ(exposure_pair.host_b.susceptibility,
            kSusceptibleVisit.susceptibility);
  EXPECT_EQ(exposure_pair.host_a.location_transmissibility,
            kLocationTransmissibility);
  EXPECT_EQ(exposure_pair.host_b.location_transmissibility,
            kLocationTransmissibility);

  // Ensure the exposure pair is filled correctly in reverse order.
  exposure_pair =
      generator.Generate(kLocationTransmissibility, kSusceptibleVisit,
                         kInfectiousSymptomaticVisit);
  EXPECT_EQ(exposure_pair.host_a.infectivity,
            kInfectiousSymptomaticVisit.infectivity);
  EXPECT_EQ(exposure_pair.host_b.infectivity, kSusceptibleVisit.infectivity);
  EXPECT_EQ(exposure_pair.host_a.symptom_factor,
            kInfectiousSymptomaticVisit.symptom_factor);
  EXPECT_EQ(exposure_pair.host_b.symptom_factor,
            kSusceptibleVisit.symptom_factor);
  EXPECT_EQ(exposure_pair.host_a.susceptibility,
            kSusceptibleVisit.susceptibility);
  EXPECT_EQ(exposure_pair.host_b.susceptibility,
            kInfectiousSymptomaticVisit.susceptibility);
  EXPECT_EQ(exposure_pair.host_a.location_transmissibility,
            kLocationTransmissibility);
  EXPECT_EQ(exposure_pair.host_b.location_transmissibility,
            kLocationTransmissibility);
}

TEST(TripleExposureGeneratorTest, CorrectDataIsMirrored) {
  TripleExposureGenerator generator;
  ExposurePair exposure_pair =
      generator.Generate(kLocationTransmissibility, kInfectiousSymptomaticVisit,
                         kSusceptibleVisit);
  EXPECT_EQ(exposure_pair.host_a.start_time, exposure_pair.host_b.start_time);
  EXPECT_EQ(exposure_pair.host_a.duration, exposure_pair.host_b.duration);
  EXPECT_EQ(exposure_pair.host_a.distance, exposure_pair.host_b.distance);
  EXPECT_EQ(exposure_pair.host_a.attenuation, exposure_pair.host_b.attenuation);
  EXPECT_EQ(exposure_pair.host_a.location_transmissibility,
            exposure_pair.host_b.location_transmissibility);
}

}  // namespace
}  // namespace abesim
