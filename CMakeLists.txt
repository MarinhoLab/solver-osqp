# https://pybind11.readthedocs.io/en/stable/compiling.html
cmake_minimum_required(VERSION 3.18...3.29)

if(WIN32)
    set(CMAKE_TOOLCHAIN_FILE C:/vcpkg/scripts/buildsystems/vcpkg.cmake)
endif()

include(cmake/GetVersion.cmake)
project(marinholab_solver_osqp VERSION ${SOLVER_OSQP_VERSION} LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

option(BUILD_EXAMPLES "Build the standalone C++ example (example/)." OFF)
# The pybind11 Python extension is built via `pip install .` / setup.py,
# which passes -DBUILD_PYTHON=ON; the C++-only build (Debian package,
# find_package consumers) leaves it OFF.
option(BUILD_PYTHON "Build the pybind11 Python extension module (_core)." OFF)
# Shared library by default; build a static library with -DBUILD_SHARED_LIBS=OFF.
# The option() establishes the default while still honouring a value that a
# parent project (or the command line) provides.
option(BUILD_SHARED_LIBS "Build the marinholab_osqp shared library" ON)

find_package(Eigen3 REQUIRED)

# https://stackoverflow.com/questions/38296756/what-is-the-idiomatic-way-in-cmake-to-add-the-fpic-compiler-option
set(CMAKE_POSITION_INDEPENDENT_CODE ON)
# OSQP is always built as a *static* library: its archive is bundled (installed)
# with our package rather than shipped as a separate shared object. The Python
# build additionally links it into the pybind11 module, so our own library must
# be static there as well; the C++-only build lets marinholab_osqp follow
# BUILD_SHARED_LIBS (shared by default).
if(BUILD_PYTHON)
    set(BUILD_SHARED_LIBS FALSE CACHE BOOL "x" FORCE)
endif()
# We only need the static library, and none of OSQP's own demos/tests. The
# built-in algebra embeds its own linear-solver (qdldl), so the static archive
# is self-contained (only the C++ runtime and Eigen headers are needed at
# runtime), which is what lets the Debian package bundle it with no extra
# shared-library dependencies.
set(OSQP_BUILD_SHARED_LIB FALSE CACHE BOOL "x" FORCE)
set(OSQP_BUILD_STATIC_LIB TRUE CACHE BOOL "x" FORCE)
set(OSQP_BUILD_DEMO_EXE FALSE CACHE BOOL "x" FORCE)
set(OSQP_BUILD_UNITTESTS FALSE CACHE BOOL "x" FORCE)
# OSQP ships a code-generation tool; we only use its solver, so turn codegen
# off to keep the built and installed tree small.
set(OSQP_CODEGEN FALSE CACHE BOOL "x" FORCE)
# OSQP's built-in algebra pulls in its linear-solver (qdldl) via
# FetchContent. Point that at the vendored submodule so no network access is
# needed at configure time (reproducible, offline-friendly builds).
set(FETCHCONTENT_SOURCE_DIR_QDLDL "${CMAKE_CURRENT_SOURCE_DIR}/qdldl" CACHE STRING "x")
add_subdirectory(osqp)

# The Solver/Configuration implementation. Both the pybind11 module and the
# standalone example link against it, so the C++ code is compiled exactly once.
# Shared by default; static with -DBUILD_SHARED_LIBS=OFF.
add_library(marinholab_osqp
        src/core_function.cpp
    )
add_library(marinholab::solvers::osqp ALIAS marinholab_osqp)

set_target_properties(marinholab_osqp PROPERTIES
    OUTPUT_NAME marinholab_solver_osqp
    POSITION_INDEPENDENT_CODE ON
    VERSION ${PROJECT_VERSION}
    SOVERSION "${PROJECT_VERSION_MAJOR}.${PROJECT_VERSION_MINOR}")

target_include_directories(marinholab_osqp
    PUBLIC
        ${PROJECT_SOURCE_DIR}/include
    )

# The vendored OSQP is a private implementation detail: its static archive is
# linked into this package, so consumers only ever link the
# marinholab::solvers::osqp target. The public header is OSQP-free, so its
# headers are not even exposed to consumers.
target_link_libraries(marinholab_osqp
    PUBLIC Eigen3::Eigen
    PRIVATE osqpstatic)

# Build the standalone C++ example (see example/example.cpp).
if (BUILD_EXAMPLES)
    add_executable(example_osqp example/example.cpp)
    target_link_libraries(example_osqp PRIVATE marinholab_osqp)
endif ()

# Python extension (built only for `pip install .` / setup.py).
if(BUILD_PYTHON)
    set(PYBIND11_FINDPYTHON ON)
    add_subdirectory(pybind11)
    pybind11_add_module(_core MODULE
            src/core.cpp
        )
    target_link_libraries(_core PRIVATE marinholab_osqp)
    # Version is dynamically obtained. See pyproject.toml
    # Activating this causes issues with Windows compilation.
    # target_compile_definitions(_core PRIVATE VERSION_INFO=${PROJECT_VERSION})
endif()

# ---- Install / CMake package (C++ part only) ----
# Runs only under `cmake --install` (e.g. from the Debian package build); the
# `pip install .` path uses build_ext and never invokes the install rules.
include(GNUInstallDirs)
include(CMakePackageConfigHelpers)

# Values passed to the package config as @PACKAGE_<VAR>@ (see PATH_VARS below).
# Using the actual install dirs keeps the config correct on multiarch Debian
# (libdir = lib/<arch>, includedir = include).
set(INCLUDE_INSTALL_DIR ${CMAKE_INSTALL_INCLUDEDIR})
set(LIB_INSTALL_DIR ${CMAKE_INSTALL_LIBDIR})

install(TARGETS marinholab_osqp
        ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR}
        LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
        RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR})

install(DIRECTORY include/ DESTINATION ${CMAKE_INSTALL_INCLUDEDIR})

if(BUILD_EXAMPLES)
    install(TARGETS example_osqp RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR})
endif ()

configure_package_config_file(
    ${CMAKE_CURRENT_SOURCE_DIR}/cmake/marinholab_solver_osqp-config.cmake.in
    ${CMAKE_CURRENT_BINARY_DIR}/marinholab_solver_osqp-config.cmake
    INSTALL_DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/marinholab_solver_osqp
    PATH_VARS INCLUDE_INSTALL_DIR LIB_INSTALL_DIR)

write_basic_package_version_file(
    ${CMAKE_CURRENT_BINARY_DIR}/marinholab_solver_osqp-config-version.cmake
    VERSION ${PROJECT_VERSION}
    COMPATIBILITY SameMajorVersion)

install(FILES
    ${CMAKE_CURRENT_BINARY_DIR}/marinholab_solver_osqp-config.cmake
    ${CMAKE_CURRENT_BINARY_DIR}/marinholab_solver_osqp-config-version.cmake
    DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/marinholab_solver_osqp)
