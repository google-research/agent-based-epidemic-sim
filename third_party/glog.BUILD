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

licenses(["notice"])

exports_files(["LICENSE"])

cc_library(
    name = "glog",
    srcs = [
        "config_h",
        "src/base/commandlineflags.h",
        "src/base/googleinit.h",
        "src/base/mutex.h",
        "src/demangle.cc",
        "src/demangle.h",
        "src/logging.cc",
        "src/raw_logging.cc",
        "src/signalhandler.cc",
        "src/symbolize.cc",
        "src/symbolize.h",
        "src/utilities.cc",
        "src/utilities.h",
        "src/vlog_is_on.cc",
    ] + glob(["src/stacktrace*.h"]),
    hdrs = [
        "src/glog/log_severity.h",
        "src/glog/logging.h",
        "src/glog/raw_logging.h",
        "src/glog/stl_logging.h",
        "src/glog/vlog_is_on.h",
    ],
    copts = [
        "-Wno-sign-compare",
        "-U_XOPEN_SOURCE",
    ],
    includes = ["./src"],
    linkopts = ["-lpthread"],
    visibility = ["//visibility:public"],
    deps = ["@com_github_gflags_gflags//:gflags"],
)

genrule(
    name = "run_configure",
    srcs = [
        "README",
        "Makefile.in",
        "config.guess",
        "config.sub",
        "install-sh",
        "ltmain.sh",
        "missing",
        "libglog.pc.in",
        "src/config.h.in",
        "src/glog/logging.h.in",
        "src/glog/raw_logging.h.in",
        "src/glog/stl_logging.h.in",
        "src/glog/vlog_is_on.h.in",
    ],
    outs = [
        "config.h.tmp",
        "src/glog/logging.h.tmp",
        "src/glog/raw_logging.h",
        "src/glog/stl_logging.h",
        "src/glog/vlog_is_on.h",
    ],
    cmd = "$(location :configure)" +
          "&& cp -v src/config.h $(location config.h.tmp) " +
          "&& cp -v src/glog/logging.h $(location src/glog/logging.h.tmp) " +
          "&& cp -v src/glog/raw_logging.h $(location src/glog/raw_logging.h) " +
          "&& cp -v src/glog/stl_logging.h $(location src/glog/stl_logging.h) " +
          "&& cp -v src/glog/vlog_is_on.h $(location src/glog/vlog_is_on.h) ",
    tools = [
        "configure",
    ],
)

genrule(
    name = "config_h",
    srcs = ["config.h.tmp"],
    outs = ["config.h"],
    cmd = "echo select $< to be the glog config file. && cp $< $@",
)

genrule(
    name = "logging_h",
    srcs = ["src/glog/logging.h.tmp"],
    outs = ["src/glog/logging.h"],
    cmd = "echo select $< to be the glog logging.h file. && cp $< $@",
)
