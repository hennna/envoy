package(default_visibility = ["//visibility:public"])

load("//bazel:envoy_build_system.bzl", "envoy_cc_library")

envoy_cc_library(
    name = "base_includes",
    hdrs = [
        "exception.h",
        "pure.h",
    ],
)

envoy_cc_library(
    name = "time_includes",
    hdrs = ["time.h"],
    deps = [":base_includes"],
)

envoy_cc_library(
    name = "optional_includes",
    hdrs = ["optional.h"],
)
