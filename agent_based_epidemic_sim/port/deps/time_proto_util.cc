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

#include "absl/status/status.h"
#include "absl/status/statusor.h"
#include "absl/strings/str_cat.h"
#include "absl/time/time.h"
#include "agent_based_epidemic_sim/core/integral_types.h"
#include "google/protobuf/timestamp.pb.h"

namespace abesim {

namespace {

// Validation requirements documented at:
// (broken link)
absl::Status Validate(const google::protobuf::Duration& d) {
  const auto sec = d.seconds();
  const auto ns = d.nanos();
  if (sec < -315576000000 || sec > 315576000000) {
    return absl::Status(absl::StatusCode::kInvalidArgument,
                        absl::StrCat("seconds=", sec));
  }
  if (ns < -999999999 || ns > 999999999) {
    return absl::Status(absl::StatusCode::kInvalidArgument,
                        absl::StrCat("nanos=", ns));
  }
  if ((sec < 0 && ns > 0) || (sec > 0 && ns < 0)) {
    return absl::Status(absl::StatusCode::kInvalidArgument, "sign mismatch");
  }
  return absl::OkStatus();
}

// Validation requirements documented at:
// (broken link)
absl::Status Validate(const google::protobuf::Timestamp& t) {
  const auto sec = t.seconds();
  const auto ns = t.nanos();
  // sec must be [0001-01-01T00:00:00Z, 9999-12-31T23:59:59.999999999Z]
  if (sec < -62135596800 || sec > 253402300799) {
    return absl::Status(absl::StatusCode::kInvalidArgument,
                        absl::StrCat("seconds=", sec));
  }
  if (ns < 0 || ns > 999999999) {
    return absl::Status(absl::StatusCode::kInvalidArgument,
                        absl::StrCat("nanos=", ns));
  }
  return absl::OkStatus();
}

}  // namespace

absl::Status EncodeGoogleApiProto(absl::Duration d,
                                  google::protobuf::Duration* proto) {
  // s and n may both be negative, per the Duration proto spec.
  const int64 s = absl::IDivDuration(d, absl::Seconds(1), &d);
  const int64 n = absl::IDivDuration(d, absl::Nanoseconds(1), &d);
  proto->set_seconds(s);
  proto->set_nanos(n);
  return Validate(*proto);
}

absl::Status EncodeGoogleApiProto(absl::Time t,
                                  google::protobuf::Timestamp* proto) {
  const int64 s = absl::ToUnixSeconds(t);
  proto->set_seconds(s);
  proto->set_nanos((t - absl::FromUnixSeconds(s)) / absl::Nanoseconds(1));
  return Validate(*proto);
}

absl::StatusOr<absl::Duration> DecodeGoogleApiProto(
    const google::protobuf::Duration& proto) {
  absl::Status status = Validate(proto);
  if (!status.ok()) return status;
  return absl::Seconds(proto.seconds()) + absl::Nanoseconds(proto.nanos());
}

absl::StatusOr<absl::Time> DecodeGoogleApiProto(
    const google::protobuf::Timestamp& proto) {
  absl::Status status = Validate(proto);
  if (!status.ok()) return status;
  return absl::FromUnixSeconds(proto.seconds()) +
         absl::Nanoseconds(proto.nanos());
}

}  // namespace abesim
