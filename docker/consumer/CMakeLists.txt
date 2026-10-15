cmake_minimum_required(VERSION 3.16)
project(marinholab_solver_osqp_consumer CXX)

# C++17 is all the public header needs (std::variant in Configuration); the
# package itself exports no compile-feature requirements.
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# The point of this project: a *downstream* project that knows nothing about
# the source tree and only uses what `apt-get install
# libmarinholab-solver-osqp` (or `cmake --install`) put on the system.
find_package(marinholab_solver_osqp REQUIRED)

# A config file is re-processed on every find_package() call, so one that
# creates its imported target unconditionally dies with "another target with
# the same name already exists" in any project that resolves the package more
# than once (a superbuild, or several components each calling find_package).
# ON checks that the config file is re-entrant.
option(DUPLICATE_FIND_PACKAGE "Resolve the package a second time" OFF)
if(DUPLICATE_FIND_PACKAGE)
    find_package(marinholab_solver_osqp REQUIRED)
endif()

add_executable(consumer main.cpp)
target_link_libraries(consumer PRIVATE marinholab::solvers::osqp)
