cmake_minimum_required(VERSION 3.21)

project(hanji
    VERSION 0.1.0
    DESCRIPTION "Runtime clock and 3D presentation for eight-direction sprite characters"
    LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

option(HANJI_BUILD_TESTS "Build the Hanji core unit tests" ON)
option(HANJI_WARNINGS_AS_ERRORS "Treat compiler warnings as errors" OFF)
option(HANJI_SANITIZE "Build with AddressSanitizer and UndefinedBehaviorSanitizer" OFF)

# Shared warning and sanitizer settings, applied per target. Third-party code
# is built without the warning flags but with the sanitizer flags.
add_library(hanji_sanitize INTERFACE)
add_library(hanji_options INTERFACE)
target_link_libraries(hanji_options INTERFACE hanji_sanitize)
if(MSVC)
    target_compile_options(hanji_options INTERFACE /W4 /permissive-)
    if(HANJI_WARNINGS_AS_ERRORS)
        target_compile_options(hanji_options INTERFACE /WX)
    endif()
else()
    target_compile_options(hanji_options INTERFACE
        -Wall -Wextra -Wpedantic -Wshadow -Wconversion -Wsign-conversion)
    # No fused multiply-add contraction, so clock and geometry results are the
    # same in the last bit on every architecture (the same choice as pose-atlas).
    target_compile_options(hanji_sanitize INTERFACE -ffp-contract=off)
    if(HANJI_WARNINGS_AS_ERRORS)
        target_compile_options(hanji_options INTERFACE -Werror)
    endif()
    if(HANJI_SANITIZE)
        target_compile_options(hanji_sanitize INTERFACE
            -fsanitize=address,undefined -fno-omit-frame-pointer -fno-sanitize-recover=all)
        target_link_options(hanji_sanitize INTERFACE -fsanitize=address,undefined)
    endif()
endif()

add_subdirectory(third_party)

# The engine-independent core: data, clock and geometry. No Unreal types, so it
# builds and tests anywhere; the Unreal plugin under unreal/ wraps it.
add_library(hanji_core STATIC
    core/src/clip.cpp
    core/src/readers.cpp
    core/src/version.cpp)
add_library(hanji::core ALIAS hanji_core)
target_include_directories(hanji_core PUBLIC
    $<BUILD_INTERFACE:${PROJECT_SOURCE_DIR}/core/include>)
target_compile_definitions(hanji_core PRIVATE
    HANJI_VERSION_STRING="${PROJECT_VERSION}")
target_link_libraries(hanji_core
    PRIVATE $<BUILD_INTERFACE:hanji_json> $<BUILD_INTERFACE:hanji_options>)

if(HANJI_BUILD_TESTS)
    enable_testing()
    add_subdirectory(core/tests)
endif()
