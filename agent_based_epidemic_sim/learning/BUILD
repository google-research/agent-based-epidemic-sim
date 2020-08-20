licenses(["notice"])

py_binary(
    name = "infectiousness_lookup_table",
    srcs = ["infectiousness_lookup_table.py"],
    python_version = "PY3",
    deps = [
        "//third_party/py/absl:app",
        "//third_party/py/absl/flags",
        "//third_party/py/jax",
        "//third_party/py/numpy",
        "//third_party/py/pandas",
        "//third_party/py/scipy",
    ],
)
