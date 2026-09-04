# https://pybind11.readthedocs.io/en/stable/compiling.html
cmake_minimum_required(VERSION 3.18...3.29)

if(WIN32)
    set(CMAKE_TOOLCHAIN_FILE C:/vcpkg/scripts/buildsystems/vcpkg.cmake)
endif()

project(marinholab_solver_osqp LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

option(BUILD_EXAMPLES "Build the standalone C++ example (example/)." OFF)

find_package(Eigen3 REQUIRED)

# https://stackoverflow.com/questions/38296756/what-is-the-idiomatic-way-in-cmake-to-add-the-fpic-compiler-option
set(CMAKE_POSITION_INDEPENDENT_CODE ON)
# CMAKE was not passing BUILD_SHARED_LIBS nicely to the add_subdirectory.
# https://thatonegamedev.com/cpp/cmake/how-to-manage-dependencies-with-cmake/
set(BUILD_SHARED_LIBS FALSE CACHE BOOL "x" FORCE)

# We only need the static library, and none of OSQP's own demos/tests.
set(OSQP_BUILD_SHARED_LIB FALSE CACHE BOOL "x" FORCE)
set(OSQP_BUILD_STATIC_LIB TRUE CACHE BOOL "x" FORCE)
set(OSQP_BUILD_DEMO_EXE FALSE CACHE BOOL "x" FORCE)
set(OSQP_BUILD_UNITTESTS FALSE CACHE BOOL "x" FORCE)
add_subdirectory(osqp)

# Static library with the Solver/Configuration implementation. Both the
# pybind11 module and any standalone C++ consumer link against it, so the C++
# code is compiled exactly once.
add_library(marinholab_osqp STATIC
        src/core_function.cpp
    )

target_include_directories(marinholab_osqp
    PUBLIC
        ${PROJECT_SOURCE_DIR}/include
        osqp/include/public
    )

target_link_libraries(marinholab_osqp PUBLIC Eigen3::Eigen osqpstatic)

# Build the standalone C++ example (see example/example.cpp).
if (BUILD_EXAMPLES)
    add_executable(example_osqp example/example.cpp)
    target_link_libraries(example_osqp PRIVATE marinholab_osqp)
    set_target_properties(example_osqp PROPERTIES
        RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/example)
endif ()

# Python extension.
set(PYBIND11_FINDPYTHON ON)
add_subdirectory(pybind11)
pybind11_add_module(_core MODULE
        src/core.cpp
    )

target_link_libraries(_core PRIVATE marinholab_osqp)
# Version is dynamically obtained. See pyproject.toml
# Activating this causes issues with Windows compilation.
# target_compile_definitions(_core PRIVATE VERSION_INFO=${PROJECT_VERSION})
