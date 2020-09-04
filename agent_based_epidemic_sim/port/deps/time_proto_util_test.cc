// Copyright 2020 Google LLC
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//      http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#include "agent_based_epidemic_sim/port/deps/time_proto_util.h"

#include <limits>

#include "absl/time/time.h"
#include "agent_based_epidemic_sim/core/integral_types.h"
#include "agent_based_epidemic_sim/port/deps/status_matchers.h"
#include "gmock/gmock.h"
#include "google/protobuf/timestamp.pb.h"
#include "google/protobuf/util/message_differencer.h"
#include "gtest/gtest.h"

namespace abesim {

google::protobuf::Duration MakeGoogleApiDuration(int64 s, int32 ns) {
  google::protobuf::Duration proto;
  proto.set_seconds(s);
  proto.set_nanos(ns);
  return proto;
}

// Returns the max absl::Duration that can be represented as
// google::protobuf::Duration. Same as
// DecodeGoogleApiProto(MakeGoogleApiDurationProtoMax()).value().
inline absl::Duration MakeGoogleApiDurationMax() {
  return absl::Seconds(315576000000) + absl::Nanoseconds(999999999);
}

// Returns the min absl::Duration that can be represented as
// google::protobuf::Duration. Same as
// DecodeGoogleApiProto(MakeGoogleApiDurationProtoMin()).value().
inline absl::Duration MakeGoogleApiDurationMin() {
  return absl::Seconds(-315576000000) + absl::Nanoseconds(-999999999);
}

// Returns the max Timestamp proto representing 9999-12-31T23:59:59.999999999Z,
// as defined by: (broken link)
inline google::protobuf::Timestamp MakeGoogleApiTimestampProtoMax() {
  google::protobuf::Timestamp proto;
  proto.set_seconds(253402300799);
  proto.set_nanos(999999999);
  return proto;
}

// Returns the min Timestamp proto representing 0001-01-01T00:00:00Z,
// as defined by: (broken link)
inline google::protobuf::Timestamp MakeGoogleApiTimestampProtoMin() {
  google::protobuf::Timestamp proto;
  proto.set_seconds(-62135596800);
  proto.set_nanos(0);
  return proto;
}

inline absl::Time MakeGoogleApiTimeMax() {
  return absl::UnixEpoch() + absl::Seconds(253402300799) +
         absl::Nanoseconds(999999999);
}

// Returns the min absl::Time that can be represented as
// google::protobuf::Timestamp. Same as
// DecodeGoogleApiProto(MakeGoogleApiTimestampProtoMin()).value().
inline absl::Time MakeGoogleApiTimeMin() {
  return absl::UnixEpoch() + absl::Seconds(-62135596800);
}

google::protobuf::Timestamp MakeGoogleApiTimestamp(int64 s, int32 ns) {
  google::protobuf::Timestamp proto;
  proto.set_seconds(s);
  proto.set_nanos(ns);
  return proto;
}

// Helper function that tests the EncodeGoogleApiProto() and
// DecodeGoogleApiProto() functions. Both variants of the EncodeGoogleApiProto
// are tested to ensure they return the proto result. The templated first
// argument may be either a absl::Duration or a absl::Time. The template P
// represents a google::protobuf::Duration or google::protobuf::Timestamp.
template <typename T, typename P>
void RoundTripGoogleApi(T v, int64 expected_sec, int32 expected_nsec) {
  // auto status = EncodeGoogleApiProto(v,
  // const auto sor_proto = EncodeGoogleApiProto(v);
  // ASSERT_OK(sor_proto);
  // const auto& proto = sor_proto.value();
  // EXPECT_EQ(proto.seconds(), expected_sec);
  // EXPECT_EQ(proto.nanos(), expected_nsec);

  P out_proto;
  const auto status = EncodeGoogleApiProto(v, &out_proto);
  PANDEMIC_ASSERT_OK(status);
  EXPECT_EQ(out_proto.seconds(), expected_sec);
  EXPECT_EQ(out_proto.nanos(), expected_nsec);
  // EXPECT_THAT(proto, testing::EqualsProto(out_proto));

  // Complete the round-trip by decoding the proto back to a absl::Duration.
  const auto sor_duration = DecodeGoogleApiProto(out_proto);
  PANDEMIC_ASSERT_OK(sor_duration);
  const auto& duration = sor_duration.value();
  EXPECT_EQ(duration, v);
}

// // Helper function that tests the EncodeGoogleApiProto() and
// // DecodeGoogleApiProto() functions. Both variants of the
// EncodeGoogleApiProto
// // are tested to ensure they return the proto result.
// void RoundTripGoogleApi(absl::Time v, int64 expected_sec, int32
// expected_nsec) {
//   google::protobuf::Timestamp proto;
//   const auto status = EncodeGoogleApiProto(v, &proto);
//   PANDEMIC_ASSERT_OK(status);
//   EXPECT_EQ(proto.seconds(), expected_sec);
//   EXPECT_EQ(proto.nanos(), expected_nsec);

//   // Complete the round-trip by decoding the proto back to a absl::Timestamp.
//   const auto sor_timestamp = DecodeGoogleApiProto(proto);
//   PANDEMIC_ASSERT_OK(sor_timestamp);
//   const auto& timestamp = sor_timestamp.value();
//   EXPECT_EQ(timestamp, v);
// }

TEST(ProtoUtilGoogleApi, RoundTripDuration) {
  // Shorthand to make the test cases readable.
  const auto& s = [](int64 n) { return absl::Seconds(n); };
  const auto& ns = [](int64 n) { return absl::Nanoseconds(n); };
  const struct {
    absl::Duration d;
    struct {
      int64 sec;
      int32 nsec;
    } expected;
  } kTestCases[] = {
      {s(0), {0, 0}},
      {s(123) + ns(456), {123, 456}},
      {ns(-5), {0, -5}},
      {s(-10) - ns(5), {-10, -5}},
      {s(-315576000000), {-315576000000, 0}},
      {s(315576000000), {315576000000, 0}},
      {MakeGoogleApiDurationMin(), {-315576000000, -999999999}},
      {MakeGoogleApiDurationMax(), {315576000000, 999999999}},
  };

  for (const auto& tc : kTestCases) {
    RoundTripGoogleApi<absl::Duration, google::protobuf::Duration>(
        tc.d, tc.expected.sec, tc.expected.nsec);
  }
}

TEST(ProtoUtilGoogleApi, RoundTripTime) {
  // Shorthand to make the test cases readable.
  const absl::Time epoch = absl::UnixEpoch();  // The protobuf epoch.
  const auto& s = [](int64 n) { return absl::Seconds(n); };
  const auto& ns = [](int64 n) { return absl::Nanoseconds(n); };
  const struct {
    absl::Time t;
    struct {
      int64 sec;
      int32 nsec;
    } expected;
  } kTestCases[] = {
      {epoch, {0, 0}},
      {epoch - ns(1), {-1, 999999999}},
      {epoch + ns(1), {0, 1}},
      {epoch + s(123) + ns(456), {123, 456}},
      {epoch - ns(5), {-1, 999999995}},
      {epoch - s(10) - ns(5), {-11, 999999995}},
      {MakeGoogleApiTimeMin(), {-62135596800, 0}},
      {MakeGoogleApiTimeMax(), {253402300799, 999999999}},
  };

  for (const auto& tc : kTestCases) {
    RoundTripGoogleApi<absl::Time, google::protobuf::Timestamp>(
        tc.t, tc.expected.sec, tc.expected.nsec);
  }
}

TEST(ProtoUtilGoogleApi, TimeTruncTowardInfPast) {
  const absl::Duration tick = absl::Nanoseconds(1) / 4;
  const absl::Time before_epoch = absl::FromUnixSeconds(-1234567890);
  const absl::Time epoch = absl::UnixEpoch();
  const absl::Time after_epoch = absl::FromUnixSeconds(1234567890);
  const struct {
    absl::Time t;
    struct {
      int64 sec;
      int32 nsec;
    } expected;
  } kTestCases[] = {
      {before_epoch + tick, {-1234567890, 0}},
      {before_epoch - tick, {-1234567890 - 1, 999999999}},
      {epoch + tick, {0, 0}},
      {epoch - tick, {-1, 999999999}},
      {after_epoch + tick, {1234567890, 0}},
      {after_epoch - tick, {1234567890 - 1, 999999999}},
      {MakeGoogleApiTimeMin() + tick, {-62135596800, 0}},
      {MakeGoogleApiTimeMax() - tick, {253402300799, 999999998}},
      {MakeGoogleApiTimeMax() + tick, {253402300799, 999999999}},
  };

  for (const auto& tc : kTestCases) {
    google::protobuf::Timestamp proto;
    PANDEMIC_ASSERT_OK(EncodeGoogleApiProto(tc.t, &proto));
    EXPECT_EQ(proto.seconds(), tc.expected.sec) << "t=" << tc.t;
    EXPECT_EQ(proto.nanos(), tc.expected.nsec) << "t=" << tc.t;
  }
}

TEST(ProtoUtilGoogleApi, EncodeTimeError) {
  const absl::Time kTestCases[] = {
      MakeGoogleApiTimeMin() - absl::Nanoseconds(1),  //
      MakeGoogleApiTimeMax() + absl::Nanoseconds(1),  //
      absl::InfinitePast(),                           //
      absl::InfiniteFuture(),                         //
  };

  for (const auto& t : kTestCases) {
    google::protobuf::Timestamp proto;
    EXPECT_FALSE(EncodeGoogleApiProto(t, &proto).ok()) << "t=" << t;
  }
}

TEST(ProtoUtilGoogleApi, DecodeTimeError) {
  const google::protobuf::Timestamp kTestCases[] = {
      MakeGoogleApiTimestamp(1, -1),             //
      MakeGoogleApiTimestamp(1, 999999999 + 1),  //
      MakeGoogleApiTimestamp(std::numeric_limits<int64>::lowest(), 0),
      MakeGoogleApiTimestamp(std::numeric_limits<int64>::max(), 0),
      MakeGoogleApiTimestamp(0, std::numeric_limits<int32>::lowest()),
      MakeGoogleApiTimestamp(0, std::numeric_limits<int32>::max()),
      MakeGoogleApiTimestamp(std::numeric_limits<int64>::lowest(),
                             std::numeric_limits<int32>::lowest()),
      MakeGoogleApiTimestamp(std::numeric_limits<int64>::max(),
                             std::numeric_limits<int32>::max()),
      MakeGoogleApiTimestamp(
          ToUnixSeconds(MakeGoogleApiTimeMin() - absl::Seconds(1)), 0),
      MakeGoogleApiTimestamp(
          absl::ToUnixSeconds(MakeGoogleApiTimeMax() + absl::Seconds(1)), 0),
  };

  for (const auto& d : kTestCases) {
    const auto sor = DecodeGoogleApiProto(d);
    EXPECT_FALSE(sor.ok()) << "d=" << d.DebugString();
  }
}

TEST(ProtoUtilGoogleApi, TimestampProtoMax) {
  const auto proto_max = MakeGoogleApiTimestampProtoMax();
  const absl::Time time_max = MakeGoogleApiTimeMax();
  EXPECT_THAT(DecodeGoogleApiProto(proto_max), abesim::IsOkAndHolds(time_max));
  google::protobuf::Timestamp proto;
  PANDEMIC_EXPECT_OK(EncodeGoogleApiProto(time_max, &proto));
  EXPECT_TRUE(
      google::protobuf::util::MessageDifferencer::Equals(proto, proto_max));
}

TEST(ProtoUtilGoogleApi, TimeMaxIsOK) {
  const absl::Time time_max = MakeGoogleApiTimeMax();
  const absl::TimeZone utc = absl::UTCTimeZone();
  const absl::Time other_time_max =
      absl::FromDateTime(9999, 12, 31, 23, 59, 59, utc) +
      absl::Nanoseconds(999999999);
  EXPECT_EQ(time_max, other_time_max);
}

TEST(ProtoUtilGoogleApi, TimeMaxIsMax) {
  const absl::Time time_max = MakeGoogleApiTimeMax();
  google::protobuf::Timestamp proto;
  EXPECT_THAT(EncodeGoogleApiProto(time_max + absl::Nanoseconds(1), &proto),
              abesim::StatusIs(absl::StatusCode::kInvalidArgument));
}

TEST(ProtoUtilGoogleApi, TimestampProtoMin) {
  const auto proto_min = MakeGoogleApiTimestampProtoMin();
  const absl::Time time_min = MakeGoogleApiTimeMin();

  EXPECT_THAT(DecodeGoogleApiProto(proto_min), abesim::IsOkAndHolds(time_min));
  google::protobuf::Timestamp proto;
  PANDEMIC_EXPECT_OK(EncodeGoogleApiProto(time_min, &proto));
  EXPECT_TRUE(
      google::protobuf::util::MessageDifferencer::Equals(proto, proto_min));
}

TEST(ProtoUtilGoogleApi, TimeMinIsOK) {
  const absl::Time time_min = MakeGoogleApiTimeMin();
  const absl::TimeZone utc = absl::UTCTimeZone();
  const absl::Time other_time_min = absl::FromDateTime(1, 1, 1, 0, 0, 0, utc);
  EXPECT_EQ(time_min, other_time_min);
}

TEST(ProtoUtilGoogleApi, TimeMinIsMin) {
  const absl::Time time_min = MakeGoogleApiTimeMin();
  google::protobuf::Timestamp proto;
  EXPECT_THAT(EncodeGoogleApiProto(time_min - absl::Nanoseconds(1), &proto),
              abesim::StatusIs(absl::StatusCode::kInvalidArgument));
}

}  // namespace abesim
