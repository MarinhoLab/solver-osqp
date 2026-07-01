# https://pybind11.readthedocs.io/en/stable/compiling.html
cmake_minimum_required(VERSION 3.18...3.29)

if(WIN32)
    set(CMAKE_TOOLCHAIN_FILE C:/vcpkg/scripts/buildsystems/vcpkg.cmake)
endif()

project(marinholab_solver_osqp LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

find_package(Eigen3 REQUIRED)

set(PYBIND11_FINDPYTHON ON)
add_subdirectory(pybind11)
pybind11_add_module(_core MODULE
        src/core.cpp
        src/core_function.cpp
    )

target_include_directories(_core PRIVATE
    ${PROJECT_SOURCE_DIR}/include
    )

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

target_link_libraries(_core PRIVATE Eigen3::Eigen osqpstatic)
# Version is dynamically obtained. See pyproject.toml
# Activating this causes issues with Windows compilation.
# target_compile_definitions(_core PRIVATE VERSION_INFO=${PROJECT_VERSION})
