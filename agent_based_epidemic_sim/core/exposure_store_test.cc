#include "agent_based_epidemic_sim/core/exposure_store.h"

#include <vector>

#include "absl/time/time.h"
#include "agent_based_epidemic_sim/core/event.h"
#include "gmock/gmock.h"
#include "gtest/gtest.h"

namespace abesim {
namespace {

absl::Time TestDay(int day) {
  return absl::UnixEpoch() + absl::Hours(24) * day;
}
absl::Time TestHour(int day, int hour) {
  return TestDay(day) + absl::Hours(hour);
}

std::vector<InfectionOutcome> Outcomes(int day, std::vector<int64> uuids) {
  std::vector<InfectionOutcome> outcomes;
  const absl::Duration duration = absl::Hours(1);
  absl::Time start = TestDay(day);
  for (int64 uuid : uuids) {
    outcomes.push_back({.exposure = {.start_time = start, .duration = duration},
                        .source_uuid = uuid});
    start += duration;
  }
  return outcomes;
}

TEST(ExposureStoreTest, AddsAndRemovesExposures) {
  ExposureStore store;

  store.AddExposures(Outcomes(2, {10, 13, 11, 12, 11}));
  store.AddExposures(Outcomes(3, {10, 10}));
  store.AddExposures(Outcomes(4, {11, 12}));

  EXPECT_EQ(store.size(), 9);

  auto get_agents = [&store](absl::Time since) {
    std::vector<int64> agents;
    store.PerAgent(since, [&agents](int64 uuid) { agents.push_back(uuid); });
    return agents;
  };
  EXPECT_THAT(get_agents(TestDay(2)),
              testing::UnorderedElementsAre(10, 11, 12, 13));
  EXPECT_THAT(get_agents(TestHour(2, 2)),
              testing::UnorderedElementsAre(10, 11, 12));
  EXPECT_THAT(get_agents(TestDay(4)), testing::UnorderedElementsAre(11, 12));

  auto get_exposures = [&store](absl::Time since, int64 uuid) {
    std::vector<absl::Time> times;
    store.PerExposure(uuid, since, [&times](const Exposure& exposure) {
      times.push_back(exposure.start_time);
    });
    return times;
  };
  EXPECT_THAT(get_exposures(TestDay(2), 10),
              testing::UnorderedElementsAre(TestHour(2, 0), TestHour(3, 0),
                                            TestHour(3, 1)));
  EXPECT_THAT(get_exposures(TestDay(2), 11),
              testing::UnorderedElementsAre(TestHour(2, 2), TestHour(2, 4),
                                            TestHour(4, 0)));
  EXPECT_THAT(get_exposures(TestDay(2), 12),
              testing::UnorderedElementsAre(TestHour(2, 3), TestHour(4, 1)));
  EXPECT_THAT(get_exposures(TestDay(3), 10),
              testing::UnorderedElementsAre(TestHour(3, 0), TestHour(3, 1)));

  store.GarbageCollect(TestHour(3, 1));

  EXPECT_EQ(store.size(), 3);

  EXPECT_THAT(get_agents(TestDay(2)),
              testing::UnorderedElementsAre(10, 11, 12));
  EXPECT_THAT(get_exposures(TestDay(2), 10),
              testing::UnorderedElementsAre(TestHour(3, 1)));
  EXPECT_THAT(get_exposures(TestDay(2), 11),
              testing::UnorderedElementsAre(TestHour(4, 0)));
  EXPECT_THAT(get_exposures(TestDay(2), 12),
              testing::UnorderedElementsAre(TestHour(4, 1)));
}

}  // namespace
}  // namespace abesim
