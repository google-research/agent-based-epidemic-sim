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

load("@rules_python//python:defs.bzl", "py_binary", "py_library", "py_test")
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

py_library(
    name = "ens_simulator",
    srcs = ["ens_simulator.py"],
    srcs_version = "PY3",
    deps = [
        requirement("numpy"),
        requirement("scipy"),
    ],
)

py_test(
    name = "ens_simulator_test",
    srcs = ["ens_simulator_test.py"],
    python_version = "PY3",
    srcs_version = "PY3",
    deps = [
        ":ens_simulator",
        requirement("absl-py"),
        requirement("six"),
    ],
)

py_library(
    name = "abesim_data_loader",
    srcs = ["abesim_data_loader.py"],
    srcs_version = "PY3",
    deps = [
        "//agent_based_epidemic_sim/applications/risk_learning:exposures_per_test_result_py_pb2",
        "//agent_based_epidemic_sim/core:pandemic_py_pb2",
        "@com_google_riegeli//python/riegeli",
    ],
)

py_test(
    name = "abesim_data_loader_test",
    srcs = ["abesim_data_loader_test.py"],
    data = [
        "testdata/fake_data_size_30",
    ],
    python_version = "PY3",
    srcs_version = "PY3",
    deps = [
        ":abesim_data_loader",
        "//agent_based_epidemic_sim/applications/risk_learning:exposures_per_test_result_py_pb2",
        "@com_google_protobuf//:protobuf_python",
        requirement("absl-py"),
        "@com_google_riegeli//python/riegeli",
    ],
)
