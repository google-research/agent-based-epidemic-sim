# Libraries for contact tracing simulation.

package(default_visibility = [
    "//agent_based_epidemic_sim:internal",
])

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

proto_library(
    name = "exposures_per_test_result_proto",
    srcs = ["exposures_per_test_result.proto"],
    deps = [
        "//agent_based_epidemic_sim/core:pandemic_proto",
        "@com_google_protobuf//:duration_proto",
        "@com_google_protobuf//:timestamp_proto",
    ],
)

cc_proto_library(
    name = "config_cc_proto",
    deps = [":config_proto"],
)

cc_proto_library(
    name = "exposures_per_test_result_cc_proto",
    deps = [":exposures_per_test_result_proto"],
)

cc_library(
    name = "risk_score",
    srcs = ["risk_score.cc"],
    hdrs = ["risk_score.h"],
    deps = [
        ":config_cc_proto",
        "//agent_based_epidemic_sim/core:integral_types",
        "//agent_based_epidemic_sim/core:location_type",
        "//agent_based_epidemic_sim/core:pandemic_cc_proto",
        "//agent_based_epidemic_sim/core:risk_score",
        "//agent_based_epidemic_sim/port:time_proto_util",
        "@com_google_absl//absl/memory",
        "@com_google_absl//absl/status:statusor",
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
        "//agent_based_epidemic_sim/core:agent",
        "//agent_based_epidemic_sim/core:duration_specified_visit_generator",
        "//agent_based_epidemic_sim/core:event",
        "//agent_based_epidemic_sim/core:graph_location",
        "//agent_based_epidemic_sim/core:hazard_transmission_model",
        "//agent_based_epidemic_sim/core:location_type",
        "//agent_based_epidemic_sim/core:micro_exposure_generator",
        "//agent_based_epidemic_sim/core:parameter_distribution_cc_proto",
        "//agent_based_epidemic_sim/core:ptts_transition_model",
        "//agent_based_epidemic_sim/core:random",
        "//agent_based_epidemic_sim/core:risk_score",
        "//agent_based_epidemic_sim/core:seir_agent",
        "//agent_based_epidemic_sim/core:simulation",
        "//agent_based_epidemic_sim/core:transition_model",
        "//agent_based_epidemic_sim/core:transmission_model",
        "//agent_based_epidemic_sim/core:visit",
        "//agent_based_epidemic_sim/core:visit_generator",
        "//agent_based_epidemic_sim/port:time_proto_util",
        "//agent_based_epidemic_sim/util:records",
        "@com_google_absl//absl/container:fixed_array",
        "@com_google_absl//absl/container:flat_hash_map",
        "@com_google_absl//absl/memory",
        "@com_google_absl//absl/random:bit_gen_ref",
        "@com_google_absl//absl/status",
        "@com_google_absl//absl/status:statusor",
        "@com_google_absl//absl/strings",
    ],
)

cc_test(
    name = "risk_score_test",
    srcs = ["risk_score_test.cc"],
    deps = [
        ":config_cc_proto",
        ":risk_score",
        "//agent_based_epidemic_sim/core:location_type",
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
        "//agent_based_epidemic_sim/util:records",
        "@com_google_absl//absl/flags:flag",
        "@com_google_absl//absl/status",
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
