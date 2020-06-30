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

#ifndef THIRD_PARTY_AGENT_BASED_EPIDEMIC_SIM_PORT_DEPS_LOGGING_H_
#define THIRD_PARTY_AGENT_BASED_EPIDEMIC_SIM_PORT_DEPS_LOGGING_H_

#include <sstream>

#include "absl/base/attributes.h"
#include "absl/base/log_severity.h"

namespace abesim {
namespace logging_internal {

// Class representing a log message created by a log macro.
class LogMessage {
 public:
  // Constructs a new message with INFO severity.
  //
  // file: source file that produced the log.
  // line: source code line that produced the log.
  LogMessage(const char *file, int line);

  // Constructs a new message with the specified severity.
  //
  // file: source file that produced the log.
  // line: source code line that produced the log.
  // severity: severity level of the log.
  LogMessage(const char *file, int line, absl::LogSeverity severity);

  // Constructs a log message with additional text that is provided by CHECK
  // macros.  Severity is implicitly FATAL.
  //
  // file: source file that produced the log.
  // line: source code line that produced the log.
  // result: result message of the failed check.
  LogMessage(const char *file, int line, const std::string &result);

  // The destructor flushes the message.
  ~LogMessage();

  LogMessage(const LogMessage &) = delete;
  void operator=(const LogMessage &) = delete;

  // Gets a reference to the underlying string stream.
  std::ostream &stream() { return stream_; }

 protected:
  void Flush();

 private:
  void Init(const char *file, int line, absl::LogSeverity severity);

  // Sends the message to print.
  void SendToLog(const std::string &message_text);

  // stream_ reads all the input messages into a stringstream, then it's
  // converted into a string in the destructor for printing.
  std::ostringstream stream_;
  const absl::LogSeverity severity_;
};

// This class is used just to take an ostream type and make it a void type to
// satisfy the ternary operator in LOG_IF.
// operator& is used because it has precedence lower than << but higher than :?
class LogMessageVoidify {
 public:
  void operator&(const std::ostream &) {}
};

// Default LogSeverity FATAL version of LogMessage.
// Identical to LogMessage(..., FATAL), but see comments on destructor.
class LogMessageFatal : public LogMessage {
 public:
  // Constructs a new message with FATAL severity.
  //
  // file: source file that produced the log.
  // line: source code line that produced the log.
  LogMessageFatal(const char *file, int line)
      : LogMessage(file, line, absl::LogSeverity::kFatal) {}

  // Constructs a message with FATAL severity for use by CHECK macros.
  //
  // file: source file that produced the log.
  // line: source code line that produced the log.
  // result: result message when check fails.
  LogMessageFatal(const char *file, int line, const std::string &result)
      : LogMessage(file, line, result) {}

  // Suppresses warnings in some cases, example:
  // if (impossible)
  //   LOG(FATAL)
  // else
  //   return 0;
  // which would otherwise yield the following compiler warning.
  // "warning: control reaches end of non-void function [-Wreturn-type]"
  ABSL_ATTRIBUTE_NORETURN ~LogMessageFatal();
};

}  // namespace logging_internal

}  // namespace abesim

#endif  // THIRD_PARTY_AGENT_BASED_EPIDEMIC_SIM_PORT_DEPS_LOGGING_H_
