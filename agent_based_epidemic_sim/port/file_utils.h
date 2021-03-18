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

#ifndef AGENT_BASED_EPIDEMIC_SIM_PORT_FILE_UTILS_H_
#define AGENT_BASED_EPIDEMIC_SIM_PORT_FILE_UTILS_H_

#include "absl/status/status.h"
#include "absl/strings/match.h"
#include "absl/strings/string_view.h"

namespace abesim {
namespace file {

// An interface for writing files.
class FileWriter {
 public:
  virtual ~FileWriter() = default;
  // Writes a string to file.
  virtual absl::Status WriteString(absl::string_view content) = 0;
  // Must be called before destroying the object.
  virtual absl::Status Close() = 0;
};

// Opens a file for writing. Crashes if the file already exists.
std::unique_ptr<FileWriter> OpenOrDie(absl::string_view file_name);

// Opens a file for writing.
std::unique_ptr<FileWriter> OpenOrDie(absl::string_view file_name,
                                      const bool fail_if_file_exists);

// Gets the contents of a file.
absl::Status GetContents(absl::string_view file_name, std::string* output);

}  // namespace file
}  // namespace abesim

#endif  //  AGENT_BASED_EPIDEMIC_SIM_PORT_FILE_UTILS_H_
