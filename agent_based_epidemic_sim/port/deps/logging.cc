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

#include "agent_based_epidemic_sim/port/deps/logging.h"

#include <errno.h>
#include <fcntl.h>
#include <libgen.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>

#include <cctype>
#include <cstdio>
#include <cstdlib>
#include <ctime>
#include <sstream>
#include <string>

#include "absl/base/attributes.h"

namespace abesim {

constexpr char kDefaultDirectory[] = "/tmp/";

namespace {

// The logging directory.
ABSL_CONST_INIT std::string *log_file_directory = nullptr;

// The log filename.
ABSL_CONST_INIT std::string *log_basename = nullptr;

const char *GetBasename(const char *file_path) {
  const char *slash = strrchr(file_path, '/');
  return slash ? slash + 1 : file_path;
}

std::string get_log_basename() {
  if (!log_basename || log_basename->empty()) {
    return "pandemic";
  }
  return *log_basename;
}

std::string get_log_directory() {
  if (!log_file_directory) {
    return kDefaultDirectory;
  }
  return *log_file_directory;
}

}  // namespace

namespace logging_internal {

LogMessage::LogMessage(const char *file, int line)
    : LogMessage(file, line, absl::LogSeverity::kInfo) {}

LogMessage::LogMessage(const char *file, int line, const std::string &result)
    : LogMessage(file, line, absl::LogSeverity::kFatal) {
  stream() << "Check failed: " << result << " ";
}

static constexpr const char *LogSeverityNames[4] = {"INFO", "WARNING", "ERROR",
                                                    "FATAL"};

LogMessage::LogMessage(const char *file, int line, absl::LogSeverity severity)
    : severity_(severity) {
  const char *filename = GetBasename(file);

  // Write a prefix into the log message, including local date/time, severity
  // level, filename, and line number.
  struct timespec time_stamp;
  clock_gettime(CLOCK_REALTIME, &time_stamp);

  constexpr int kTimeMessageSize = 22;
  char buffer[kTimeMessageSize];
  strftime(buffer, kTimeMessageSize, "%Y-%m-%d %H:%M:%S  ",
           localtime(&time_stamp.tv_sec));
  stream() << buffer;
  stream() << LogSeverityNames[static_cast<int>(severity)] << "  " << filename
           << " : " << line << " : ";
}

LogMessage::~LogMessage() {
  Flush();
  // if FATAL occurs, abort.
  if (severity_ == absl::LogSeverity::kFatal) {
    abort();
  }
}

void LogMessage::SendToLog(const std::string &message_text) {
  std::string log_path = get_log_directory() + get_log_basename();

  FILE *file = fopen(log_path.c_str(), "ab");
  if (file) {
    if (fprintf(file, "%s", message_text.c_str()) > 0) {
      if (message_text.back() != '\n') {
        fprintf(file, "\n");
      }
    } else {
      fprintf(stderr, "Failed to write to log file : %s! [%s]\n",
              log_path.c_str(), strerror(errno));
    }
    fclose(file);
  } else {
    fprintf(stderr, "Failed to open log file : %s! [%s]\n", log_path.c_str(),
            strerror(errno));
  }
  if (severity_ >= absl::LogSeverity::kError) {
    fprintf(stderr, "%s\n", message_text.c_str());
    fflush(stderr);
  }
  printf("%s\n", message_text.c_str());
  fflush(stdout);
}

void LogMessage::Flush() {
  std::string message_text = stream_.str();
  SendToLog(message_text);
  stream_.clear();
}

LogMessageFatal::~LogMessageFatal() {
  Flush();
  abort();
}

}  // namespace logging_internal

}  // namespace abesim
