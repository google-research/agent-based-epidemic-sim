# Libraries for contact tracing simulation.

licenses(["notice"])

proto_library(
    name = "config_proto",
    srcs = ["config.proto"],
    deps = [
        "//agent_based_epidemic_sim/applications/home_work:config_proto",
        "@com_google_protobuf//:duration_proto",
    ],
)

cc_proto_library(
    name = "config_cc_proto",
    deps = [":config_proto"],
)

cc_library(
    name = "public_policy",
    srcs = ["public_policy.cc"],
    hdrs = ["public_policy.h"],
    deps = [
        ":config_cc_proto",
        "//agent_based_epidemic_sim/applications/home_work:simulation",
        "//agent_based_epidemic_sim/core:integral_types",
        "//agent_based_epidemic_sim/core:public_policy",
        "//agent_based_epidemic_sim/port:statusor",
        "//agent_based_epidemic_sim/port:time_proto_util",
        "@com_google_absl//absl/memory",
        "@com_google_absl//absl/time",
    ],
)

cc_library(
    name = "simulation",
    srcs = ["simulation.cc"],
    hdrs = ["simulation.h"],
    deps = [
        ":config_cc_proto",
        ":public_policy",
        "//agent_based_epidemic_sim/applications/home_work:simulation",
        "//agent_based_epidemic_sim/core:public_policy",
        "@com_google_absl//absl/strings",
    ],
)

cc_test(
    name = "public_policy_test",
    srcs = ["public_policy_test.cc"],
    deps = [
        ":config_cc_proto",
        ":public_policy",
        "//agent_based_epidemic_sim/core:parse_text_proto",
        "//agent_based_epidemic_sim/core:public_policy",
        "//agent_based_epidemic_sim/port:status_matchers",
        "//agent_based_epidemic_sim/util:ostream_overload",
        "@com_google_absl//absl/time",
        "@com_google_googletest//:gtest_main",
    ],
)

cc_test(
    name = "simulation_test",
    srcs = ["simulation_test.cc"],
    data = [
        ":config.pbtxt",
    ],
    deps = [
        ":config_cc_proto",
        ":simulation",
        "//agent_based_epidemic_sim/applications/home_work:config_cc_proto",
        "//agent_based_epidemic_sim/core:parse_text_proto",
        "//agent_based_epidemic_sim/core:public_policy",
        "//agent_based_epidemic_sim/port:file_utils",
        "//agent_based_epidemic_sim/port:status_matchers",
        "@com_google_absl//absl/flags:flag",
        "@com_google_absl//absl/strings",
        "@com_google_absl//absl/time",
        "@com_google_googletest//:gtest_main",
    ],
)

cc_binary(
    name = "main",
    srcs = ["main.cc"],
    deps = [
        ":config_cc_proto",
        ":simulation",
        "//agent_based_epidemic_sim/core:parse_text_proto",
        "//agent_based_epidemic_sim/port:file_utils",
        "//agent_based_epidemic_sim/port:logging",
        "@com_google_absl//absl/flags:flag",
        "@com_google_absl//absl/flags:parse",
    ],
)

cc_library(
    name = "graph_location",
    srcs = ["graph_location.cc"],
    hdrs = ["graph_location.h"],
    deps = [
        "//agent_based_epidemic_sim/core:event",
        "//agent_based_epidemic_sim/core:integral_types",
        "//agent_based_epidemic_sim/core:location",
        "@com_google_absl//absl/container:flat_hash_map",
        "@com_google_absl//absl/random",
        "@com_google_absl//absl/random:distributions",
    ],
)

cc_test(
    name = "graph_location_test",
    srcs = ["graph_location_test.cc"],
    deps = [
        ":graph_location",
        "//agent_based_epidemic_sim/core:event",
        "//agent_based_epidemic_sim/core:pandemic_cc_proto",
        "@com_google_googletest//:gtest_main",
    ],
)
