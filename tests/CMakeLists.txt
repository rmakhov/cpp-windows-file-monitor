# ---------------------------------------------------------------------------
# GoogleTest
# ---------------------------------------------------------------------------

include(FetchContent)

FetchContent_Declare(
    googletest
    GIT_REPOSITORY https://github.com/google/googletest.git
    GIT_TAG v1.17.0
)

# Prevent GoogleTest from overriding our compiler/runtime settings on MSVC.
set(gtest_force_shared_crt ON CACHE BOOL "" FORCE)

FetchContent_MakeAvailable(googletest)

# ---------------------------------------------------------------------------
# Unit tests
# ---------------------------------------------------------------------------

add_executable(
    cwm-unit-tests

    unit/smoke_test.cpp
)

target_link_libraries(
    cwm-unit-tests
    PRIVATE
        cwm-iocp
        GTest::gtest_main
)

target_link_libraries(
    cwm-unit-tests
    PRIVATE
        cwm-compiler-options
)

include(GoogleTest)

gtest_discover_tests(cwm-unit-tests)
