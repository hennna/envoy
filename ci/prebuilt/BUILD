package(default_visibility = ["//visibility:public"])

cc_library(
    name = "googletest_main",
    srcs = [
        "thirdparty_build/lib/libgmock.a",
        "thirdparty_build/lib/libgtest.a",
        "thirdparty_build/lib/libgtest_main.a",
    ],
    hdrs = glob([
        "thirdparty_build/include/gmock/**/*.h",
        "thirdparty_build/include/gtest/**/*.h",
    ]),
    strip_include_prefix = "thirdparty_build/include",
)

cc_library(
    name = "spdlog",
    hdrs = glob([
        "thirdparty/spdlog-0.11.0/include/**/*.cc",
        "thirdparty/spdlog-0.11.0/include/**/*.h",
    ]),
    strip_include_prefix = "thirdparty/spdlog-0.11.0/include",
)
