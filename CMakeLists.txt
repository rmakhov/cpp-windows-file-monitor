cmake_minimum_required(VERSION 3.25)

project(
cpp-windows-file-monitor
VERSION 0.1.0
DESCRIPTION "Production-oriented Windows filesystem monitoring framework built with C++20, IOCP, and ReadDirectoryChangesW"
LANGUAGES CXX
)

# ---------------------------------------------------------------------------
# Project-wide configuration
# ---------------------------------------------------------------------------

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# Export compile_commands.json for tooling and static analysis.
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

# ---------------------------------------------------------------------------
# Common compiler configuration
# ---------------------------------------------------------------------------

add_library(cwm-compiler-options INTERFACE)

target_compile_features(
cwm-compiler-options
INTERFACE
cxx_std_20
)

target_compile_definitions(
cwm-compiler-options
INTERFACE
WIN32_LEAN_AND_MEAN
NOMINMAX
UNICODE
_UNICODE
)

if(MSVC)
target_compile_options(
cwm-compiler-options
INTERFACE
/W4
/permissive-
/EHsc
/utf-8
)
endif()

# ---------------------------------------------------------------------------
# Platform library
# ---------------------------------------------------------------------------

add_library(
    cwm-platform
    STATIC
        src/platform/win32_handle.cpp
)

target_link_libraries(
    cwm-platform
    PUBLIC
        cwm-compiler-options
)

target_include_directories(
    cwm-platform
    PUBLIC
        ${PROJECT_SOURCE_DIR}/include
)

# ---------------------------------------------------------------------------
# IOCP library
# ---------------------------------------------------------------------------

#
# The reusable asynchronous I/O infrastructure will live here.
#

# Initially this target is intentionally empty. Source files will be added
# as the IOCP layer is implemented.
#

add_library(
    cwm-iocp
    STATIC
        src/iocp/iocp.cpp
        src/iocp/iocp_context.cpp
        src/iocp/iocp_operation.cpp
)

target_link_libraries(
	cwm-iocp
	PUBLIC
		cwm-platform
		cwm-compiler-options
)

target_include_directories(
	cwm-iocp
	PUBLIC
		${PROJECT_SOURCE_DIR}/include
)

# Windows libraries required by the IOCP layer will be added here as needed.
#

# target_link_libraries(
# cwm-iocp
# PRIVATE
# ...
# )

# ---------------------------------------------------------------------------
# Filesystem monitoring library
# ---------------------------------------------------------------------------

#
# ReadDirectoryChangesW-based monitoring will live here.
#

add_library(
    cwm-filesystem
    STATIC
        src/filesystem/filesystem.cpp
)

target_link_libraries(
	cwm-filesystem
	PUBLIC
		cwm-iocp
		cwm-compiler-options
)

target_include_directories(
	cwm-filesystem
	PUBLIC
		${PROJECT_SOURCE_DIR}/include
)

# ---------------------------------------------------------------------------
# Monitoring framework
# ---------------------------------------------------------------------------

#
# Higher-level monitoring orchestration will live here.
#

add_library(
    cwm-monitor
    STATIC
        src/monitor/monitor.cpp
)

target_link_libraries(
	cwm-monitor
	PUBLIC
		cwm-filesystem
		cwm-compiler-options
)

target_include_directories(
	cwm-monitor
	PUBLIC
		${PROJECT_SOURCE_DIR}/include
)

# ---------------------------------------------------------------------------
# Example application
# ---------------------------------------------------------------------------

#
# Enabled once the first executable is implemented.
#

option(
CWM_BUILD_EXAMPLES
"Build example applications"
ON
)

if(CWM_BUILD_EXAMPLES)

add_executable(
    cwm-file-monitor
    examples/basic_monitor/main.cpp
)

target_link_libraries(
    cwm-file-monitor
    PRIVATE
        cwm-monitor
)

endif()

# ---------------------------------------------------------------------------
# Tests
# ---------------------------------------------------------------------------

option(
    CWM_BUILD_TESTS
    "Build tests"
    ON
)

if(CWM_BUILD_TESTS)
    enable_testing()
    add_subdirectory(tests)
endif()

# ---------------------------------------------------------------------------
# Installation
# ---------------------------------------------------------------------------

#
# Installation rules will be introduced once the public API stabilizes.
#
# ---------------------------------------------------------------------------
