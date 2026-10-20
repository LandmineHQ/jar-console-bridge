cmake_minimum_required(VERSION 3.16)

project(jar_console_bridge_native VERSION 1.0.0 LANGUAGES CXX)

if(NOT UNIX)
    message(FATAL_ERROR "The native console bridge currently targets POSIX systems only.")
endif()

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

if(NOT CMAKE_BUILD_TYPE AND NOT CMAKE_CONFIGURATION_TYPES)
    set(CMAKE_BUILD_TYPE Release CACHE STRING "Build type" FORCE)
endif()

option(SHIM_WARNINGS_AS_ERRORS "Treat compiler warnings as errors" OFF)
option(SHIM_STATIC_CXX_RUNTIME "Statically link the GCC C++ and support runtimes" ON)
option(SHIM_FULLY_STATIC "Statically link the C++ runtime and libc" OFF)

set(SHIM_RESOURCE_DIR "${CMAKE_CURRENT_SOURCE_DIR}/../src/main/resources" CACHE PATH
    "Directory containing shell resources shared with the Java launcher")

set(SHIM_AUTO_LAUNCH_FILE "${SHIM_RESOURCE_DIR}/server__autoLaunch.sh")
set(SHIM_PATH_FILE "${SHIM_RESOURCE_DIR}/server__path.sh")

foreach(resource IN ITEMS "${SHIM_AUTO_LAUNCH_FILE}" "${SHIM_PATH_FILE}")
    if(NOT EXISTS "${resource}")
        message(FATAL_ERROR "Required shell resource is missing: ${resource}")
    endif()
endforeach()

# Embed the exact files used by the Maven build so both distributions always
# share one source of truth for startup commands and PATH configuration.
file(READ "${SHIM_AUTO_LAUNCH_FILE}" SHIM_AUTO_LAUNCH_SCRIPT)
file(READ "${SHIM_PATH_FILE}" SHIM_PATH_SCRIPT)

set(SHIM_GENERATED_DIR "${CMAKE_CURRENT_BINARY_DIR}/generated")
file(MAKE_DIRECTORY "${SHIM_GENERATED_DIR}")
configure_file(
    "${CMAKE_CURRENT_SOURCE_DIR}/src/embedded_resources.hpp.in"
    "${SHIM_GENERATED_DIR}/embedded_resources.hpp"
    @ONLY
)

add_executable(server src/main.cpp)
target_include_directories(server PRIVATE "${SHIM_GENERATED_DIR}" "${CMAKE_CURRENT_SOURCE_DIR}/src")

if(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
    target_compile_options(server PRIVATE -Wall -Wextra -Wpedantic)
    if(SHIM_WARNINGS_AS_ERRORS)
        target_compile_options(server PRIVATE -Werror)
    endif()
endif()

if(SHIM_FULLY_STATIC AND CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
    target_link_options(server PRIVATE -static)
elseif(SHIM_STATIC_CXX_RUNTIME AND CMAKE_CXX_COMPILER_ID STREQUAL "GNU")
    target_link_options(server PRIVATE -static-libstdc++ -static-libgcc)
endif()

install(TARGETS server RUNTIME DESTINATION .)
