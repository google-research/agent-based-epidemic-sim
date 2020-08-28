# Utililites for pandemic package.

licenses(["notice"])

package(default_visibility = [
    "//agent_based_epidemic_sim:internal",
])

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
