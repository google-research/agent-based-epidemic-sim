# Libraries for synthesizing agents.

licenses(["notice"])

package(
    default_applicable_licenses = ["//agent_based_epidemic_sim:license"],
    default_visibility = [
        "//agent_based_epidemic_sim:internal",
    ],
)

proto_library(
    name = "population_proto",
    srcs = ["population.proto"],
    visibility = ["//visibility:public"],
)

cc_proto_library(
    name = "population_cc_proto",
    visibility = ["//visibility:public"],
    deps = [":population_proto"],
)

proto_library(
    name = "population_profile_proto",
    srcs = ["population_profile.proto"],
    deps = [
        "//agent_based_epidemic_sim/core:pandemic_proto",
        "//agent_based_epidemic_sim/core:parameter_distribution_proto",
        "//agent_based_epidemic_sim/core:ptts_transition_model_proto",
    ],
)

cc_proto_library(
    name = "population_profile_cc_proto",
    deps = [":population_profile_proto"],
)

cc_library(
    name = "shuffled_sampler",
    srcs = ["shuffled_sampler.cc"],
    hdrs = ["shuffled_sampler.h"],
    deps = [
        ":population_profile_cc_proto",
        "//agent_based_epidemic_sim/core:distribution_sampler",
        "//agent_based_epidemic_sim/core:enum_indexed_array",
        "//agent_based_epidemic_sim/core:integral_types",
        "//agent_based_epidemic_sim/core:parameter_distribution_cc_proto",
        "//agent_based_epidemic_sim/core:random",
        "//agent_based_epidemic_sim/core:uuid_generator",
        "//agent_based_epidemic_sim/port:logging",
        "@com_google_absl//absl/container:flat_hash_map",
        "@com_google_absl//absl/random",
    ],
)

cc_library(
    name = "agent_sampler",
    srcs = ["agent_sampler.cc"],
    hdrs = ["agent_sampler.h"],
    deps = [
        ":population_profile_cc_proto",
        ":shuffled_sampler",
        "//agent_based_epidemic_sim/core:distribution_sampler",
        "//agent_based_epidemic_sim/core:enum_indexed_array",
        "//agent_based_epidemic_sim/core:pandemic_cc_proto",
        "//agent_based_epidemic_sim/core:parameter_distribution_cc_proto",
        "//agent_based_epidemic_sim/core:uuid_generator",
        "@com_google_absl//absl/random",
    ],
)
