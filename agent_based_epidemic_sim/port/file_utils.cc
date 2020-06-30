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

#include "agent_based_epidemic_sim/port/file_utils.h"

#include <filesystem>
#include <fstream>
#include <sstream>

#include "absl/strings/str_cat.h"
#include "agent_based_epidemic_sim/port/logging.h"

namespace abesim {
namespace file {
namespace {
class FileWriterImpl : public FileWriter {
 public:
  explicit FileWriterImpl(std::ofstream ofstream)
      : ofstream_(std::move(ofstream)) {}

  absl::Status WriteString(absl::string_view content) override {
    if (ofstream_.is_open()) {
      ofstream_ << content;
      return absl::OkStatus();
    }
    return absl::Status(absl::StatusCode::kUnavailable, "Failed to write.");
  }

  absl::Status Close() override {
    ofstream_.close();
    if (ofstream_.is_open()) {
      return absl::Status(absl::StatusCode::kUnknown, "Failed to close.");
    }
    return absl::OkStatus();
  }

 private:
  std::ofstream ofstream_;
};
}  // namespace

std::unique_ptr<FileWriter> OpenOrDie(absl::string_view file_name) {
  CHECK(!std::filesystem::exists(file_name))
      << "File already exists: " << file_name;
  std::ofstream ofstream((std::string(file_name)));
  return absl::make_unique<FileWriterImpl>(std::move(ofstream));
}

absl::Status GetContents(absl::string_view file_name, std::string* output) {
  std::ifstream input_file((std::string(file_name)));
  if (input_file.good()) {
    std::stringstream buffer;
    buffer << input_file.rdbuf();
    *output = buffer.str();
    return output->empty()
               ? absl::Status(absl::StatusCode::kUnavailable, "File empty.")
               : absl::OkStatus();
  } else {
    return absl::Status(absl::StatusCode::kNotFound,
                        absl::StrCat("File not found: ", file_name));
  }
}

}  // namespace file
}  // namespace abesim
