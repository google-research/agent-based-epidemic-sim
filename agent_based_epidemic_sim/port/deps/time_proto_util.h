/*
 * Copyright 2020 Google LLC
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#ifndef AGENT_BASED_EPIDEMIC_SIM_PORT_DEPS_TIME_PROTO_UTIL_H_
#define AGENT_BASED_EPIDEMIC_SIM_PORT_DEPS_TIME_PROTO_UTIL_H_

#include "absl/status/status.h"
#include "absl/time/time.h"
#include "agent_based_epidemic_sim/port/deps/statusor.h"
#include "google/protobuf/duration.pb.h"
#include "google/protobuf/timestamp.pb.h"

namespace abesim {

// Encodes an absl::Duration as a google::protobuf::Duration, following the
// encoding rules specified at (broken link)
// Returns an error if the given absl::Duration is beyond the range allowed by
// the protobuf. Otherwise, truncates toward zero with nanosecond precision and
// returns the google::protobuf::Duration.
//
// See also: (broken link)
//
// Note: +/- absl::InfiniteDuration() cannot be encoded because they are not
// representable in the protobuf.
absl::Status EncodeGoogleApiProto(absl::Duration d,
                                  google::protobuf::Duration* proto);

// Encodes an absl::Time as a google::protobuf::Timestamp, following the
// encoding rules specified at (broken link)
// Returns an error if the given absl::Time is beyond the range allowed by the
// protobuf. Otherwise, truncates toward infinite past with nanosecond precision
// and returns the google::protobuf::Timestamp.
//
// See also: (broken link)
//
// Note: absl::InfiniteFuture/absl::InfinitePast() cannot be encoded because
// they are not representable in the protobuf.
absl::Status EncodeGoogleApiProto(absl::Time t,
                                  google::protobuf::Timestamp* proto);

// Decodes the given protobuf and returns an absl::Time, or returns an error
// status if the argument is invalid according to
// (broken link)
StatusOr<absl::Time> DecodeGoogleApiProto(
    const google::protobuf::Timestamp& proto);

// Decodes the given protobuf and returns an absl::Duration, or returns an error
// status if the argument is invalid according to
// (broken link)
StatusOr<absl::Duration> DecodeGoogleApiProto(
    const google::protobuf::Duration& proto);

}  // namespace abesim

#endif  // AGENT_BASED_EPIDEMIC_SIM_PORT_DEPS_TIME_PROTO_UTIL_H_
