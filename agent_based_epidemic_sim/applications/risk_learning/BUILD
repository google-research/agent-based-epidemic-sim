# Libraries for contact tracing simulation.

licenses(["notice"])

proto_library(
    name = "config_proto",
    srcs = ["config.proto"],
    deps = [
        "//agent_based_epidemic_sim/agent_synthesis:population_profile_proto",
        "@com_google_protobuf//:duration_proto",
        "@com_google_protobuf//:timestamp_proto",
    ],
)

cc_proto_library(
    name = "config_cc_proto",
    deps = [":config_proto"],
)

cc_library(
    name = "risk_score",
    srcs = ["risk_score.cc"],
    hdrs = ["risk_score.h"],
    deps = [
        ":config_cc_proto",
        "//agent_based_epidemic_sim/applications/home_work:simulation",
        "//agent_based_epidemic_sim/core:integral_types",
        "//agent_based_epidemic_sim/core:pandemic_cc_proto",
        "//agent_based_epidemic_sim/core:risk_score",
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
        ":risk_score",
        "//agent_based_epidemic_sim/agent_synthesis:population_profile_cc_proto",
        "//agent_based_epidemic_sim/applications/home_work:simulation",
        "//agent_based_epidemic_sim/core:agent",
        "//agent_based_epidemic_sim/core:risk_score",
        "//agent_based_epidemic_sim/core:simulation",
        "//agent_based_epidemic_sim/port:time_proto_util",
        "@com_google_absl//absl/container:fixed_array",
        "@com_google_absl//absl/status",
        "@com_google_absl//absl/strings",
        "@com_google_riegeli//riegeli/bytes:fd_reader",
        "@com_google_riegeli//riegeli/records:record_reader",
    ],
)

cc_test(
    name = "risk_score_test",
    srcs = ["risk_score_test.cc"],
    deps = [
        ":config_cc_proto",
        ":risk_score",
        "//agent_based_epidemic_sim/applications/home_work:simulation",
        "//agent_based_epidemic_sim/core:parse_text_proto",
        "//agent_based_epidemic_sim/core:risk_score",
        "//agent_based_epidemic_sim/core:timestep",
        "//agent_based_epidemic_sim/port:status_matchers",
        "//agent_based_epidemic_sim/util:ostream_overload",
        "@com_google_absl//absl/time",
        "@com_google_googletest//:gtest_main",
    ],
)

cc_test(
    name = "simulation_test",
    srcs = ["simulation_test.cc"],
    data = glob(["testdata/**"]),
    deps = [
        ":config_cc_proto",
        ":simulation",
        "//agent_based_epidemic_sim/agent_synthesis:population_profile_cc_proto",
        "//agent_based_epidemic_sim/core:parse_text_proto",
        "//agent_based_epidemic_sim/core:risk_score",
        "//agent_based_epidemic_sim/port:file_utils",
        "//agent_based_epidemic_sim/port:status_matchers",
        "@com_google_absl//absl/flags:flag",
        "@com_google_absl//absl/status",
        "@com_google_absl//absl/strings",
        "@com_google_absl//absl/time",
        "@com_google_googletest//:gtest_main",
        "@com_google_riegeli//riegeli/bytes:fd_writer",
        "@com_google_riegeli//riegeli/records:record_writer",
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
