# Packages for the simplified home-work simulation application.
licenses(["notice"])

package(default_visibility = [
    "//agent_based_epidemic_sim:internal",
])

proto_library(
    name = "config_proto",
    srcs = ["config.proto"],
    deps = [
        "//agent_based_epidemic_sim/core:pandemic_proto",
        "//agent_based_epidemic_sim/core:parameter_distribution_proto",
        "//agent_based_epidemic_sim/core:ptts_transition_model_proto",
        "@com_google_protobuf//:duration_proto",
        "@com_google_protobuf//:timestamp_proto",
    ],
)

cc_proto_library(
    name = "config_cc_proto",
    deps = [":config_proto"],
)

cc_library(
    name = "learning_contacts_observer",
    srcs = [
        "learning_contacts_observer.cc",
    ],
    hdrs = [
        "learning_contacts_observer.h",
    ],
    deps = [
        "//agent_based_epidemic_sim/core:agent",
        "//agent_based_epidemic_sim/core:event",
        "//agent_based_epidemic_sim/core:health_state",
        "//agent_based_epidemic_sim/core:observer",
        "//agent_based_epidemic_sim/core:timestep",
        "//agent_based_epidemic_sim/port:file_utils",
        "@com_google_absl//absl/status",
        "@com_google_absl//absl/strings",
        "@com_google_absl//absl/types:span",
    ],
)

cc_library(
    name = "learning_history_and_testing_observer",
    srcs = [
        "learning_history_and_testing_observer.cc",
    ],
    hdrs = [
        "learning_history_and_testing_observer.h",
    ],
    deps = [
        "//agent_based_epidemic_sim/core:agent",
        "//agent_based_epidemic_sim/core:event",
        "//agent_based_epidemic_sim/core:observer",
        "//agent_based_epidemic_sim/core:pandemic_cc_proto",
        "//agent_based_epidemic_sim/core:timestep",
        "//agent_based_epidemic_sim/port:file_utils",
        "@com_google_absl//absl/status",
        "@com_google_absl//absl/strings",
        "@com_google_absl//absl/time",
        "@com_google_absl//absl/types:span",
    ],
)

cc_library(
    name = "simulation",
    srcs = [
        "observer.cc",
        "public_policy.cc",
        "simulation.cc",
    ],
    hdrs = [
        "location_type.h",
        "observer.h",
        "public_policy.h",
        "simulation.h",
    ],
    deps = [
        ":config_cc_proto",
        ":learning_contacts_observer",
        ":learning_history_and_testing_observer",
        "//agent_based_epidemic_sim/agent_synthesis:agent_sampler",
        "//agent_based_epidemic_sim/agent_synthesis:population_profile_cc_proto",
        "//agent_based_epidemic_sim/agent_synthesis:shuffled_sampler",
        "//agent_based_epidemic_sim/core:agent",
        "//agent_based_epidemic_sim/core:aggregated_transmission_model",
        "//agent_based_epidemic_sim/core:duration_specified_visit_generator",
        "//agent_based_epidemic_sim/core:enum_indexed_array",
        "//agent_based_epidemic_sim/core:integral_types",
        "//agent_based_epidemic_sim/core:location",
        "//agent_based_epidemic_sim/core:location_discrete_event_simulator",
        "//agent_based_epidemic_sim/core:micro_exposure_generator",
        "//agent_based_epidemic_sim/core:observer",
        "//agent_based_epidemic_sim/core:ptts_transition_model",
        "//agent_based_epidemic_sim/core:public_policy",
        "//agent_based_epidemic_sim/core:seir_agent",
        "//agent_based_epidemic_sim/core:simulation",
        "//agent_based_epidemic_sim/core:uuid_generator",
        "//agent_based_epidemic_sim/core:wrapped_transition_model",
        "//agent_based_epidemic_sim/port:file_utils",
        "//agent_based_epidemic_sim/port:logging",
        "//agent_based_epidemic_sim/port:proto_enum_utils",
        "//agent_based_epidemic_sim/port:statusor",
        "//agent_based_epidemic_sim/port:time_proto_util",
        "@com_google_absl//absl/container:flat_hash_map",
        "@com_google_absl//absl/container:flat_hash_set",
        "@com_google_absl//absl/memory",
        "@com_google_absl//absl/random",
        "@com_google_absl//absl/random:distributions",
        "@com_google_absl//absl/strings",
        "@com_google_absl//absl/strings:str_format",
        "@com_google_absl//absl/time",
    ],
)

cc_test(
    name = "observer_test",
    srcs = ["observer_test.cc"],
    deps = [
        ":simulation",
        "//agent_based_epidemic_sim/core:agent",
        "//agent_based_epidemic_sim/core:broker",
        "//agent_based_epidemic_sim/core:event",
        "//agent_based_epidemic_sim/core:location",
        "//agent_based_epidemic_sim/core:pandemic_cc_proto",
        "//agent_based_epidemic_sim/port:status_matchers",
        "@com_google_absl//absl/container:flat_hash_map",
        "@com_google_absl//absl/time",
        "@com_google_googletest//:gtest_main",
    ],
)

cc_test(
    name = "public_policy_test",
    srcs = ["public_policy_test.cc"],
    deps = [
        ":config_cc_proto",
        ":simulation",
        "//agent_based_epidemic_sim/core:public_policy",
        "//agent_based_epidemic_sim/port:status_matchers",
        "//agent_based_epidemic_sim/port:time_proto_util",
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
