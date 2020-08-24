# Copyright 2020 Google LLC
#
# Licensed under the Apache License, Version 2.0 (the "License");
# you may not use this file except in compliance with the License.
# You may obtain a copy of the License at
#
#      http://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing, software
# distributed under the License is distributed on an "AS IS" BASIS,
# WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
# See the License for the specific language governing permissions and
# limitations under the License.
load("@rules_python//python:defs.bzl", "py_binary")
load("@pip//:requirements.bzl", "requirement")

licenses(["notice"])

py_binary(
    name = "infectiousness_lookup_table",
    srcs = ["infectiousness_lookup_table.py"],
    python_version = "PY3",
    deps = [
        requirement("absl-py"),
        requirement("jax"),
        requirement("numpy"),
        requirement("pandas"),
        requirement("scipy"),
    ],
)
