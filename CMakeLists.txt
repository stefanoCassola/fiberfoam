cmake_minimum_required(VERSION 3.20)
project(fiberfoam VERSION 0.1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

# Build type
if(NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE Release)
endif()

# Options
option(BUILD_TESTS "Build unit and integration tests" ON)
option(BUILD_GUI "Build web GUI backend" OFF)
option(ENABLE_ONNX "Enable ONNX Runtime for ML inference" ON)

# Dependencies
find_package(Eigen3 3.3 REQUIRED NO_MODULE)
find_package(PkgConfig REQUIRED)
pkg_check_modules(FFTW3 REQUIRED fftw3)
find_package(yaml-cpp REQUIRED)
find_package(nlohmann_json REQUIRED)

if(ENABLE_ONNX)
    find_package(onnxruntime QUIET)
    if(NOT onnxruntime_FOUND)
        # Try pkg-config fallback
        pkg_check_modules(ONNXRUNTIME libonnxruntime)
        if(ONNXRUNTIME_FOUND)
            set(ONNX_AVAILABLE ON)
        else()
            message(WARNING "ONNX Runtime not found. ML prediction will be disabled.")
            set(ONNX_AVAILABLE OFF)
        endif()
    else()
        set(ONNX_AVAILABLE ON)
    endif()
else()
    set(ONNX_AVAILABLE OFF)
endif()

# Version info
configure_file(
    ${CMAKE_SOURCE_DIR}/src/libfiberfoam/common/Version.h.in
    ${CMAKE_BINARY_DIR}/generated/Version.h
)

# Core library
add_subdirectory(src/libfiberfoam)

# Applications
add_subdirectory(src/apps)

# Tests
if(BUILD_TESTS)
    enable_testing()
    find_package(GTest REQUIRED)
    add_subdirectory(tests)
endif()
