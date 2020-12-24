# Utililites for pandemic package.

licenses(["notice"])

package(default_visibility = [
    "//agent_based_epidemic_sim:internal",
])

cc_library(
    name = "histogram",
    hdrs = ["histogram.h"],
    deps = [
        "@com_google_absl//absl/strings:str_format",
    ],
)

cc_test(
    name = "histogram_test",
    srcs = ["histogram_test.cc"],
    deps = [
        ":histogram",
        "@com_google_googletest//:gtest_main",
    ],
)

cc_library(
    name = "ostream_overload",
    hdrs = ["ostream_overload.h"],
)

cc_library(
    name = "records",
    srcs = ["records.cc"],
    hdrs = ["records.h"],
    deps = [
        "@com_google_absl//absl/strings",
        "@com_google_riegeli//riegeli/bytes:fd_reader",
        "@com_google_riegeli//riegeli/bytes:fd_writer",
        "@com_google_riegeli//riegeli/records:record_reader",
        "@com_google_riegeli//riegeli/records:record_writer",
    ],
)

cc_library(
    name = "test_util",
    testonly = 1,
    hdrs = ["test_util.h"],
    deps = [
        "//agent_based_epidemic_sim/core:agent",
        "//agent_based_epidemic_sim/core:broker",
        "//agent_based_epidemic_sim/core:event",
        "//agent_based_epidemic_sim/core:integral_types",
        "//agent_based_epidemic_sim/core:location",
        "//agent_based_epidemic_sim/core:risk_score",
        "//agent_based_epidemic_sim/core:risk_score_model",
        "//agent_based_epidemic_sim/core:timestep",
        "//agent_based_epidemic_sim/core:transition_model",
        "//agent_based_epidemic_sim/core:transmission_model",
        "//agent_based_epidemic_sim/core:visit",
        "//agent_based_epidemic_sim/core:visit_generator",
        "@com_google_absl//absl/time",
        "@com_google_absl//absl/types:optional",
        "@com_google_googletest//:gtest",
    ],
)

cc_library(
    name = "time_utils",
    srcs = ["time_utils.cc"],
    hdrs = ["time_utils.h"],
    deps = [
        "@com_google_absl//absl/time",
    ],
)

cc_test(
    name = "time_utils_test",
    srcs = ["time_utils_test.cc"],
    deps = [
        ":time_utils",
        "@com_google_absl//absl/time",
        "@com_google_googletest//:gtest_main",
    ],
)
