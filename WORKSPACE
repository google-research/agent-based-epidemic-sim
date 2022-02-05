workspace(name = "agent_based_epidemic_sim")

load("@bazel_tools//tools/build_defs/repo:http.bzl", "http_archive")
http_archive(
    name = "rules_python",
    url = "https://github.com/bazelbuild/rules_python/releases/download/0.3.0/rules_python-0.3.0.tar.gz",
    sha256 = "934c9ceb552e84577b0faf1e5a2f0450314985b4d8712b2b70717dc679fdc01b",
)

load("@rules_python//python:pip.bzl", "pip_install")

# Create a central repo that knows about the dependencies needed for
# requirements.txt.
pip_install(
    name = "pip",
    requirements = "//agent_based_epidemic_sim/learning:requirements.txt",
)

skylib_version = "0.9.0"
http_archive(
    name = "bazel_skylib",
    type = "tar.gz",
    url = "https://github.com/bazelbuild/bazel-skylib/releases/download/{}/bazel_skylib-{}.tar.gz".format(skylib_version, skylib_version),
    sha256 = "1dde365491125a3db70731e25658dfdd3bc5dbdfd11b840b3e987ecf043c7ca0",
)
load("@bazel_skylib//lib:versions.bzl", "versions")
versions.check(minimum_bazel_version = "2.0.0")


# ABSL cpp library, the latest lts release is not new enough for riegeli, so
# we use master.
http_archive(
    name = "com_google_absl",
    urls = [
        "https://github.com/abseil/abseil-cpp/archive/master.zip"
    ],
    # Remove after https://github.com/abseil/abseil-cpp/issues/326 is solved.
    patches = [
        "@//third_party:com_google_absl_f863b622fe13612433fdf43f76547d5edda0c93001.diff"
    ],
    patch_args = [
        "-p1",
    ],
    strip_prefix = "abseil-cpp-master",
)

http_archive(
    name = "six_archive",
    build_file = "//third_party:six.BUILD",
    sha256 = "d16a0141ec1a18405cd4ce8b4613101da75da0e9a7aec5bdd4fa804d0e0eba73",
    strip_prefix = "six-1.12.0",
    urls = [
        "http://mirror.bazel.build/pypi.python.org/packages/source/s/six/six-1.12.0.tar.gz",
        "https://pypi.python.org/packages/source/s/six/six-1.12.0.tar.gz",  # 2018-12-10
    ],
)

http_archive(
    name = "com_google_riegeli",
    urls = ["https://github.com/google/riegeli/archive/master.zip"],
    strip_prefix = "riegeli-master",
)

# Deps needed by riegeli.
http_archive(
    name = "highwayhash",
    build_file = "//third_party:highwayhash.BUILD",
    sha256 = "cf891e024699c82aabce528a024adbe16e529f2b4e57f954455e0bf53efae585",
    strip_prefix = "highwayhash-276dd7b4b6d330e4734b756e97ccfb1b69cc2e12",
    urls = [
        "https://mirror.bazel.build/github.com/google/highwayhash/archive/276dd7b4b6d330e4734b756e97ccfb1b69cc2e12.zip",
        "https://github.com/google/highwayhash/archive/276dd7b4b6d330e4734b756e97ccfb1b69cc2e12.zip",  # 2019-02-22
    ],
)
http_archive(
    name = "net_zstd",
    build_file = "//third_party:net_zstd.BUILD",
    sha256 = "b6c537b53356a3af3ca3e621457751fa9a6ba96daf3aebb3526ae0f610863532",
    strip_prefix = "zstd-1.4.5/lib",
    urls = [
        "https://mirror.bazel.build/github.com/facebook/zstd/archive/v1.4.5.zip",
        "https://github.com/facebook/zstd/archive/v1.4.5.zip",  # 2020-05-22
    ],
)
http_archive(
    name = "org_brotli",
    sha256 = "fec5a1d26f3dd102c542548aaa704f655fecec3622a24ec6e97768dcb3c235ff",
    strip_prefix = "brotli-68f1b90ad0d204907beb58304d0bd06391001a4d",
    urls = [
        "https://mirror.bazel.build/github.com/google/brotli/archive/68f1b90ad0d204907beb58304d0bd06391001a4d.zip",
        "https://github.com/google/brotli/archive/68f1b90ad0d204907beb58304d0bd06391001a4d.zip",  # 2021-08-18
    ],
)

http_archive(
    name = "snappy",
    build_file = "//third_party:snappy.BUILD",
    sha256 = "38b4aabf88eb480131ed45bfb89c19ca3e2a62daeb081bdf001cfb17ec4cd303",
    strip_prefix = "snappy-1.1.8",
    urls = [
        "https://mirror.bazel.build/github.com/google/snappy/archive/1.1.8.zip",
        "https://github.com/google/snappy/archive/1.1.8.zip",  # 2020-01-14
    ],
)
http_archive(
    name = "crc32c",
    build_file = "//third_party:crc32.BUILD",
    sha256 = "338f1d9d95753dc3cdd882dfb6e176bbb4b18353c29c411ebcb7b890f361722e",
    strip_prefix = "crc32c-1.1.0",
    urls = [
        "https://mirror.bazel.build/github.com/google/crc32c/archive/1.1.0.zip",
        "https://github.com/google/crc32c/archive/1.1.0.zip",  # 2019-05-24
    ],
)
http_archive(
    name = "zlib",
    build_file = "//third_party:zlib.BUILD",
    sha256 = "c3e5e9fdd5004dcb542feda5ee4f0ff0744628baf8ed2dd5d66f8ca1197cb1a1",
    strip_prefix = "zlib-1.2.11",
    urls = [
        "http://mirror.bazel.build/zlib.net/fossils/zlib-1.2.11.tar.gz",
        "http://zlib.net/fossils/zlib-1.2.11.tar.gz",  # 2017-01-15
    ],
)

http_archive(
    name = "rules_cc",
    strip_prefix = "rules_cc-main",
    urls = ["https://github.com/bazelbuild/rules_cc/archive/main.zip"],
)

# GoogleTest/GoogleMock framework. Used by most unit-tests.
http_archive(
     name = "com_google_googletest",
     urls = ["https://github.com/google/googletest/archive/master.zip"],
     strip_prefix = "googletest-main",
)

# gflags needed by glog
http_archive(
    name = "com_github_gflags_gflags",
    sha256 = "6e16c8bc91b1310a44f3965e616383dbda48f83e8c1eaa2370a215057b00cabe",
    strip_prefix = "gflags-77592648e3f3be87d6c7123eb81cbad75f9aef5a",
    urls = [
        "https://mirror.bazel.build/github.com/gflags/gflags/archive/77592648e3f3be87d6c7123eb81cbad75f9aef5a.tar.gz",
        "https://github.com/gflags/gflags/archive/77592648e3f3be87d6c7123eb81cbad75f9aef5a.tar.gz",
    ],
)

# glog
http_archive(
    name = "com_github_glog_glog",
    url = "https://github.com/google/glog/archive/v0.3.5.zip",
    sha256 = "267103f8a1e9578978aa1dc256001e6529ef593e5aea38193d31c2872ee025e8",
    strip_prefix = "glog-0.3.5",
    build_file = "@//third_party:glog.BUILD",
)

http_archive(
        name = "com_google_protobuf",
            url = "https://github.com/google/protobuf/archive/v3.10.1.zip",
            sha256 = "678d91d8a939a1ef9cb268e1f20c14cd55e40361dc397bb5881e4e1e532679b1",
            strip_prefix = "protobuf-3.10.1",
        )

load("@com_google_protobuf//:protobuf_deps.bzl", "protobuf_deps")
protobuf_deps()

bind(
    name = "python_headers",
    actual = "@local_config_python//:python_headers",
)

load("@com_google_riegeli//python/riegeli:python_configure.bzl", "python_configure")

bind(
    name = "six",
    actual = "@six_archive//:six",
)

python_configure(name = "local_config_python")

register_toolchains("@local_config_python//:toolchain")
