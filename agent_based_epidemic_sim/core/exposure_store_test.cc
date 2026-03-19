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

std::vector<InfectionOutcome> Outcomes(int day, std::vector<int64_t> uuids) {
  std::vector<InfectionOutcome> outcomes;
  const absl::Duration duration = absl::Hours(1);
  absl::Time start = TestDay(day);
  for (int64_t uuid : uuids) {
    outcomes.push_back({.exposure = {.start_time = start, .duration = duration},
                        .source_uuid = uuid});
    start += duration;
  }
  return outcomes;
}

TEST(ExposureStoreTest, AddsAndRemovesExposures) {
  ExposureStore store;

  auto get_notification_exposures = [&store](int64_t uuid) {
    ContactReport report = {.from_agent_uuid = uuid};
    std::vector<absl::Time> times;
    store.ProcessNotification(report, [&times](const Exposure& exposure) {
      times.push_back(exposure.start_time);
    });
    return times;
  };

  store.AddExposures(Outcomes(2, {10, 13, 11, 12, 11}));
  store.AddExposures(Outcomes(3, {10, 10}));

  EXPECT_THAT(get_notification_exposures(11),
              testing::UnorderedElementsAre(TestHour(2, 2), TestHour(2, 4)));

  store.AddExposures(Outcomes(4, {11, 12}));

  EXPECT_THAT(get_notification_exposures(11),
              testing::UnorderedElementsAre(TestHour(4, 0)));

  EXPECT_EQ(store.size(), 9);

  auto get_agents = [&store](absl::Time since) {
    std::vector<int64_t> agents;
    store.PerAgent(since, [&agents](int64_t uuid) { agents.push_back(uuid); });
    return agents;
  };
  EXPECT_THAT(get_agents(TestDay(2)),
              testing::UnorderedElementsAre(10, 11, 12, 13));
  EXPECT_THAT(get_agents(TestHour(2, 2)),
              testing::UnorderedElementsAre(10, 11, 12));
  EXPECT_THAT(get_agents(TestDay(4)), testing::UnorderedElementsAre(11, 12));

  auto get_exposures = [&store](absl::Time since) {
    std::vector<std::tuple<int64_t, absl::Time, int64_t>> exposures;
    store.PerExposure(
        since, [&exposures](int64_t uuid, const Exposure& exposure,
                            const ContactReport* report) {
          int64_t from = report != nullptr ? report->from_agent_uuid : -1;
          exposures.push_back({uuid, exposure.start_time, from});
        });
    return exposures;
  };
  {
    std::vector<std::tuple<int64_t, absl::Time, int64_t>> expected = {
        {10, TestHour(2, 0), -1},  //
        {13, TestHour(2, 1), -1},  //
        {11, TestHour(2, 2), 11},  //
        {12, TestHour(2, 3), -1},  //
        {11, TestHour(2, 4), 11},  //
        {10, TestHour(3, 0), -1},  //
        {10, TestHour(3, 1), -1},  //
        {11, TestHour(4, 0), 11},  //
        {12, TestHour(4, 1), -1},  //
    };
    EXPECT_THAT(get_exposures(absl::InfinitePast()),
                testing::UnorderedElementsAreArray(expected));
  }
  {
    std::vector<std::tuple<int64_t, absl::Time, int64_t>> expected = {
        {10, TestHour(3, 1), -1},  //
        {11, TestHour(4, 0), 11},  //
        {12, TestHour(4, 1), -1},  //
    };
    EXPECT_THAT(get_exposures(TestHour(3, 1)),
                testing::UnorderedElementsAreArray(expected));
    store.GarbageCollect(TestHour(3, 1));
    EXPECT_THAT(get_exposures(absl::InfinitePast()),
                testing::UnorderedElementsAreArray(expected));
  }
}

}  // namespace
}  // namespace abesim
