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

package(default_visibility = [
    "//agent_based_epidemic_sim:internal",
])

exports_files(["LICENSE"])

cc_library(
    name = "timestep",
    srcs = ["timestep.cc"],
    hdrs = ["timestep.h"],
    deps = [
        "@com_google_absl//absl/time",
    ],
)

cc_library(
    name = "risk_score",
    srcs = ["risk_score.cc"],
    hdrs = ["risk_score.h"],
    deps = [
        ":event",
        ":timestep",
        "@com_google_absl//absl/memory",
        "@com_google_absl//absl/time",
        "@com_google_absl//absl/types:span",
    ],
)

cc_library(
    name = "agent",
    hdrs = [
        "agent.h",
    ],
    deps = [
        ":broker",
        ":event",
        ":integral_types",
        ":pandemic_cc_proto",
        ":timestep",
        ":visit",
        "@com_google_absl//absl/time",
        "@com_google_absl//absl/types:span",
    ],
)

cc_library(
    name = "aggregated_transmission_model",
    srcs = [
        "aggregated_transmission_model.cc",
    ],
    hdrs = [
        "aggregated_transmission_model.h",
    ],
    deps = [
        ":event",
        ":transmission_model",
        ":visit",
        "@com_google_absl//absl/random",
        "@com_google_absl//absl/random:distributions",
        "@com_google_absl//absl/types:span",
    ],
)

cc_test(
    name = "aggregated_transmission_model_test",
    srcs = [
        "aggregated_transmission_model_test.cc",
    ],
    deps = [
        ":aggregated_transmission_model",
        ":visit",
        "@com_google_absl//absl/time",
        "@com_google_googletest//:gtest_main",
    ],
)

cc_library(
    name = "distribution_sampler",
    hdrs = [
        "distribution_sampler.h",
    ],
    deps = [
        ":integral_types",
        ":parameter_distribution_cc_proto",
        "//agent_based_epidemic_sim/port:logging",
        "@com_google_absl//absl/container:flat_hash_map",
        "@com_google_absl//absl/random",
        "@com_google_absl//absl/random:distributions",
    ],
)

cc_test(
    name = "distribution_sampler_test",
    srcs = [
        "distribution_sampler_test.cc",
    ],
    deps = [
        ":distribution_sampler",
        ":integral_types",
        ":parameter_distribution_cc_proto",
        ":parse_text_proto",
        "//agent_based_epidemic_sim/agent_synthesis:population_cc_proto",
        "@com_google_googletest//:gtest_main",
    ],
)

cc_library(
    name = "enum_indexed_array",
    hdrs = ["enum_indexed_array.h"],
    deps = ["@com_google_absl//absl/meta:type_traits"],
)

cc_test(
    name = "enum_indexed_array_test",
    srcs = ["enum_indexed_array_test.cc"],
    deps = [
        ":enum_indexed_array",
        ":integral_types",
        "@com_google_googletest//:gtest_main",
    ],
)

cc_library(
    name = "event",
    hdrs = [
        "event.h",
    ],
    deps = [
        ":constants",
        ":integral_types",
        ":visit",
        "//agent_based_epidemic_sim/util:ostream_overload",
        "@com_google_absl//absl/meta:type_traits",
        "@com_google_absl//absl/time",
        "@com_google_absl//absl/types:variant",
    ],
)

cc_library(
    name = "indexed_location_visit_generator",
    srcs = [
        "indexed_location_visit_generator.cc",
    ],
    hdrs = [
        "indexed_location_visit_generator.h",
    ],
    deps = [
        ":duration_specified_visit_generator",
        ":event",
        ":risk_score",
        ":timestep",
        ":visit",
        ":visit_generator",
        "@com_google_absl//absl/random",
        "@com_google_absl//absl/random:distributions",
        "@com_google_absl//absl/time",
    ],
)

cc_test(
    name = "indexed_location_visit_generator_test",
    srcs = ["indexed_location_visit_generator_test.cc"],
    deps = [
        ":indexed_location_visit_generator",
        ":risk_score",
        ":visit",
        "@com_google_absl//absl/time",
        "@com_google_googletest//:gtest_main",
    ],
)

cc_library(
    name = "broker",
    hdrs = [
        "broker.h",
    ],
    deps = [
        ":event",
        "@com_google_absl//absl/synchronization",
        "@com_google_absl//absl/types:span",
    ],
)

cc_library(
    name = "exposure_generator",
    hdrs = [
        "exposure_generator.h",
        "exposure_generator_builder.h",
    ],
    deps = [
        ":event",
        "@com_google_absl//absl/time",
    ],
)

cc_library(
    name = "health_state",
    hdrs = ["health_state.h"],
    deps = [":pandemic_cc_proto"],
)

cc_library(
    name = "location",
    hdrs = [
        "location.h",
    ],
    deps = [
        ":broker",
        ":observer",
        ":visit",
        "@com_google_absl//absl/types:span",
    ],
)

cc_library(
    name = "graph_location",
    srcs = ["graph_location.cc"],
    hdrs = ["graph_location.h"],
    deps = [
        ":event",
        ":exposure_generator",
        ":integral_types",
        ":location",
        ":micro_exposure_generator",
        "@com_google_absl//absl/container:flat_hash_map",
        "@com_google_absl//absl/random",
        "@com_google_absl//absl/random:distributions",
        "@com_google_absl//absl/time",
    ],
)

cc_test(
    name = "graph_location_test",
    srcs = ["graph_location_test.cc"],
    deps = [
        ":event",
        ":exposure_generator",
        ":graph_location",
        ":pandemic_cc_proto",
        "@com_google_googletest//:gtest_main",
    ],
)

cc_library(
    name = "location_discrete_event_simulator",
    srcs = [
        "location_discrete_event_simulator.cc",
    ],
    hdrs = [
        "location_discrete_event_simulator.h",
    ],
    deps = [
        ":broker",
        ":exposure_generator",
        ":integral_types",
        ":location",
        ":observer",
        ":transmission_model",
        ":visit",
        "//agent_based_epidemic_sim/port:logging",
        "@com_google_absl//absl/random",
        "@com_google_absl//absl/time",
    ],
)

cc_test(
    name = "location_discrete_event_simulator_test",
    srcs = [
        "location_discrete_event_simulator_test.cc",
    ],
    deps = [
        ":broker",
        ":event",
        ":integral_types",
        ":location_discrete_event_simulator",
        ":micro_exposure_generator",
        ":observer",
        ":pandemic_cc_proto",
        ":visit",
        "@com_google_absl//absl/time",
        "@com_google_googletest//:gtest_main",
    ],
)

cc_library(
    name = "location_discrete_event_simulator_builder",
    srcs = [
        "location_discrete_event_simulator_builder.cc",
    ],
    hdrs = [
        "location_discrete_event_simulator_builder.h",
    ],
    deps = [
        ":location",
        ":location_discrete_event_simulator",
        ":micro_exposure_generator",
        ":uuid_generator",
    ],
)

cc_test(
    name = "location_discrete_event_simulator_builder_test",
    srcs = ["location_discrete_event_simulator_builder_test.cc"],
    deps = [
        ":location_discrete_event_simulator_builder",
        ":uuid_generator",
        "@com_google_googletest//:gtest_main",
    ],
)

cc_library(
    name = "micro_exposure_generator",
    srcs = [
        "micro_exposure_generator.cc",
        "micro_exposure_generator_builder.cc",
    ],
    hdrs = [
        "micro_exposure_generator.h",
        "micro_exposure_generator_builder.h",
    ],
    deps = [
        ":event",
        ":exposure_generator",
        ":parameter_distribution_cc_proto",
        "@com_google_absl//absl/memory",
        "@com_google_absl//absl/time",
    ],
)

cc_library(
    name = "ptts_transition_model",
    srcs = [
        "ptts_transition_model.cc",
    ],
    hdrs = [
        "ptts_transition_model.h",
    ],
    deps = [
        ":enum_indexed_array",
        ":event",
        ":ptts_transition_model_cc_proto",
        ":transition_model",
        ":visit",
        "@com_google_absl//absl/container:flat_hash_map",
        "@com_google_absl//absl/random",
        "@com_google_absl//absl/random:distributions",
        "@com_google_absl//absl/time",
    ],
)

cc_test(
    name = "ptts_transition_model_test",
    srcs = ["ptts_transition_model_test.cc"],
    deps = [
        ":ptts_transition_model",
        ":visit",
        "@com_google_absl//absl/time",
        "@com_google_googletest//:gtest_main",
    ],
)

cc_library(
    name = "constants",
    hdrs = ["constants.h"],
    deps = [":integral_types"],
)

cc_library(
    name = "seir_agent",
    srcs = [
        "seir_agent.cc",
    ],
    hdrs = [
        "seir_agent.h",
    ],
    deps = [
        ":agent",
        ":broker",
        ":constants",
        ":event",
        ":integral_types",
        ":risk_score",
        ":transition_model",
        ":transmission_model",
        ":visit",
        ":visit_generator",
        "//agent_based_epidemic_sim/port:logging",
        "@com_google_absl//absl/container:flat_hash_set",
        "@com_google_absl//absl/time",
        "@com_google_absl//absl/types:span",
    ],
)

cc_test(
    name = "seir_agent_test",
    srcs = ["seir_agent_test.cc"],
    deps = [
        ":broker",
        ":constants",
        ":event",
        ":integral_types",
        ":risk_score",
        ":seir_agent",
        ":timestep",
        ":transition_model",
        ":visit",
        ":visit_generator",
        "@com_google_absl//absl/time",
        "@com_google_googletest//:gtest_main",
    ],
)

cc_library(
    name = "small_world_graph",
    srcs = ["small_world_graph.cc"],
    hdrs = ["small_world_graph.h"],
    deps = [
        "//agent_based_epidemic_sim/port:logging",
        "@com_google_absl//absl/container:inlined_vector",
        "@com_google_absl//absl/random",
        "@com_google_absl//absl/random:distributions",
    ],
)

cc_test(
    name = "small_world_graph_test",
    srcs = ["small_world_graph_test.cc"],
    deps = [
        ":small_world_graph",
        "@com_google_googletest//:gtest_main",
    ],
)

cc_library(
    name = "transition_model",
    hdrs = [
        "transition_model.h",
    ],
    deps = [
        ":event",
        ":visit",
    ],
)

cc_library(
    name = "transmission_model",
    hdrs = [
        "transmission_model.h",
    ],
    deps = [
        ":event",
        ":visit",
        "@com_google_absl//absl/types:span",
    ],
)

cc_library(
    name = "visit",
    hdrs = [
        "visit.h",
    ],
    deps = [
        ":integral_types",
        ":pandemic_cc_proto",
        "@com_google_absl//absl/meta:type_traits",
        "@com_google_absl//absl/time",
    ],
)

cc_library(
    name = "visit_generator",
    hdrs = [
        "visit_generator.h",
    ],
    deps = [
        ":event",
        ":risk_score",
        ":timestep",
        ":visit",
    ],
)

cc_library(
    name = "wrapped_transition_model",
    hdrs = ["wrapped_transition_model.h"],
    deps = [
        ":event",
        ":transition_model",
        ":visit",
    ],
)

cc_library(
    name = "distributed",
    hdrs = ["distributed.h"],
    deps = [
        ":broker",
        "@com_google_absl//absl/types:span",
    ],
)

cc_library(
    name = "observer",
    srcs = ["observer.cc"],
    hdrs = ["observer.h"],
    deps = [
        ":event",
        ":timestep",
        ":visit",
        "@com_google_absl//absl/container:flat_hash_set",
        "@com_google_absl//absl/types:span",
    ],
)

cc_library(
    name = "simulation",
    srcs = [
        "simulation.cc",
    ],
    hdrs = [
        "simulation.h",
    ],
    deps = [
        ":agent",
        ":broker",
        ":distributed",
        ":event",
        ":location",
        ":observer",
        ":timestep",
        "//agent_based_epidemic_sim/port:executor",
        "//agent_based_epidemic_sim/port:logging",
        "@com_google_absl//absl/base:core_headers",
        "@com_google_absl//absl/container:fixed_array",
        "@com_google_absl//absl/container:flat_hash_map",
        "@com_google_absl//absl/memory",
        "@com_google_absl//absl/synchronization",
        "@com_google_absl//absl/types:span",
    ],
)

cc_test(
    name = "simulation_test",
    srcs = [
        "simulation_test.cc",
    ],
    deps = [
        ":agent",
        ":event",
        ":location",
        ":observer",
        ":simulation",
        ":timestep",
        "@com_google_absl//absl/container:flat_hash_map",
        "@com_google_absl//absl/synchronization",
        "@com_google_absl//absl/time",
        "@com_google_absl//absl/types:span",
        "@com_google_googletest//:gtest_main",
    ],
)

proto_library(
    name = "pandemic_proto",
    srcs = ["pandemic.proto"],
    visibility = ["//visibility:public"],
    deps = [
        "@com_google_protobuf//:duration_proto",
        "@com_google_protobuf//:timestamp_proto",
    ],
)

proto_library(
    name = "parameter_distribution_proto",
    srcs = ["parameter_distribution.proto"],
    visibility = ["//visibility:public"],
    deps = [
        "@com_google_protobuf//:any_proto",
    ],
)

cc_proto_library(
    name = "parameter_distribution_cc_proto",
    deps = [":parameter_distribution_proto"],
)

proto_library(
    name = "ptts_transition_model_proto",
    srcs = ["ptts_transition_model.proto"],
    deps = [":pandemic_proto"],
)

cc_proto_library(
    name = "ptts_transition_model_cc_proto",
    deps = [":ptts_transition_model_proto"],
)

cc_proto_library(
    name = "pandemic_cc_proto",
    visibility = ["//visibility:public"],
    deps = [":pandemic_proto"],
)

cc_library(
    name = "duration_specified_visit_generator",
    srcs = ["duration_specified_visit_generator.cc"],
    hdrs = ["duration_specified_visit_generator.h"],
    deps = [
        ":event",
        ":integral_types",
        ":risk_score",
        ":timestep",
        ":visit",
        ":visit_generator",
        "//agent_based_epidemic_sim/port:logging",
        "@com_google_absl//absl/random",
        "@com_google_absl//absl/random:distributions",
        "@com_google_absl//absl/time",
    ],
)

cc_library(
    name = "uuid_generator",
    srcs = ["uuid_generator.cc"],
    hdrs = ["uuid_generator.h"],
    deps = [
        ":integral_types",
    ],
)

cc_library(
    name = "integral_types",
    hdrs = ["integral_types.h"],
)

cc_library(
    name = "parse_text_proto",
    hdrs = [
        "parse_text_proto.h",
    ],
    deps = [
        "//agent_based_epidemic_sim/port:logging",
    ] + select({
        "//conditions:default": ["@com_google_protobuf//:protobuf"],
    }),
)

cc_test(
    name = "duration_specified_visit_generator_test",
    srcs = ["duration_specified_visit_generator_test.cc"],
    deps = [
        ":duration_specified_visit_generator",
        ":integral_types",
        ":risk_score",
        ":visit",
        "@com_google_absl//absl/time",
        "@com_google_googletest//:gtest_main",
    ],
)
