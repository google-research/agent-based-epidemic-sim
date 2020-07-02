# Ported functionality for pandemic package.

licenses(["notice"])

package(default_visibility = [
    "//agent_based_epidemic_sim:internal",
])

cc_library(
    name = "file_utils",
    srcs = [
        "file_utils.cc",
    ],
    hdrs = [
        "file_utils.h",
    ],
    linkopts = [
        "-lstdc++fs",
    ],
    deps = [
        "//agent_based_epidemic_sim/port:logging",
        "@com_google_absl//absl/status",
        "@com_google_absl//absl/strings",
    ],
)

cc_library(
    name = "logging",
    hdrs = ["logging.h"],
    deps = [
        "//agent_based_epidemic_sim/port/deps:logging",
    ],
)

cc_library(
    name = "proto_enum_utils",
    hdrs = [
        "proto_enum_utils.h",
    ],
    deps = [
        "@com_google_absl//absl/types:span",
    ] + select({
        "//conditions:default": ["@com_google_protobuf//:protobuf"],
    }),
)

cc_test(
    name = "proto_enum_utils_test",
    size = "small",
    srcs = ["proto_enum_utils_test.cc"],
    deps = [
        ":proto_enum_utils",
        "//agent_based_epidemic_sim/port/testdata:enum_enumeration_cc_proto",
        "//agent_based_epidemic_sim/port/testdata:enum_repeated_field_wrapper_cc_proto",
        "@com_google_googletest//:gtest_main",
    ],
)

cc_library(
    name = "status_matchers",
    testonly = 1,
    hdrs = ["status_matchers.h"],
    deps = [
        "//agent_based_epidemic_sim/port/deps:status_matchers",
    ],
)

cc_library(
    name = "statusor",
    hdrs = ["statusor.h"],
    deps = [
        "//agent_based_epidemic_sim/port/deps:status",
    ],
)

cc_library(
    name = "time_proto_util",
    hdrs = ["time_proto_util.h"],
    deps = [
        "//agent_based_epidemic_sim/port/deps:time_proto_util",
    ],
)

cc_library(
    name = "executor",
    srcs = ["executor.cc"],
    hdrs = ["executor.h"],
    deps = [
        "@com_google_absl//absl/memory",
        "@com_google_absl//absl/synchronization",
    ],
)
