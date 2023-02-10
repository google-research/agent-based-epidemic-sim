# Test data for ported libraries.

licenses(["notice"])

package(
    default_applicable_licenses = ["//agent_based_epidemic_sim:license"],
    default_visibility = ["//visibility:public"],
)

proto_library(
    name = "enum_enumeration_proto",
    testonly = 1,
    srcs = ["enum_enumeration.proto"],
)

cc_proto_library(
    name = "enum_enumeration_cc_proto",
    testonly = 1,
    deps = [":enum_enumeration_proto"],
)

proto_library(
    name = "enum_repeated_field_wrapper_proto",
    testonly = 1,
    srcs = ["enum_repeated_field_wrapper.proto"],
)

cc_proto_library(
    name = "enum_repeated_field_wrapper_cc_proto",
    testonly = 1,
    deps = [":enum_repeated_field_wrapper_proto"],
)
