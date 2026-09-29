# Compute the rolling version (YY.MM.NN) at configure time.
# Falls back to a hardcoded version when tools/version.sh is unavailable
# (e.g. when building from a release tarball without the tools/ directory).
#
# Named SOLVER_OSQP_VERSION (not OSQP_VERSION) on purpose: OSQP's own CMake
# also defines an internal `OSQP_VERSION` variable for its private
# version.h. Keeping the names distinct avoids clobbering OSQP's when we
# `add_subdirectory(osqp)`.
set(SOLVER_OSQP_FALLBACK_VERSION "26.09.00")

if(EXISTS "${CMAKE_CURRENT_LIST_DIR}/../tools/version.sh")
    execute_process(
        COMMAND bash "${CMAKE_CURRENT_LIST_DIR}/../tools/version.sh"
        OUTPUT_VARIABLE _solver_osqp_version
        RESULT_VARIABLE _solver_osqp_version_result
        OUTPUT_STRIP_TRAILING_WHITESPACE
        ERROR_QUIET
    )
    if(NOT _solver_osqp_version_result EQUAL 0 OR NOT _solver_osqp_version MATCHES "^[0-9]+\\.[0-9]+\\.[0-9]+$")
        set(_solver_osqp_version "${SOLVER_OSQP_FALLBACK_VERSION}")
    endif()
else()
    set(_solver_osqp_version "${SOLVER_OSQP_FALLBACK_VERSION}")
endif()

set(SOLVER_OSQP_VERSION "${_solver_osqp_version}")
message(STATUS "marinholab_solver_osqp version: ${SOLVER_OSQP_VERSION}")
