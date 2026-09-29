#!/usr/bin/env bash
# Build the C++ Debian package (.deb) in an Ubuntu noble container.
#
# Produces only the C++ part of the project (the marinholab::solvers::osqp
# library + the vendored OSQP), with -O3. The pybind11 Python extension is
# intentionally not part of the .deb (it is built by `pip install .`).
#
# Usage:
#   bash docker/build-deb.sh            # build the .deb -> ./.deb-out/
#   bash docker/build-deb.sh --test     # also install it and build/run a
#                                       # find_package consumer inside the container
set -euo pipefail

ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
TEST=0
[ "${1:-}" = "--test" ] && TEST=1

IMAGE="marinholab-solver-osqp-deb"
TAG="$(date +%Y%m%d%H%M)"

echo "=== Building ${IMAGE}:${TAG} ==="
docker build -f "${ROOT}/docker/deb.Dockerfile" -t "${IMAGE}:${TAG}" "${ROOT}"

OUT="${ROOT}/.deb-out"
mkdir -p "${OUT}"

WORKDIR="/opt/solver-osqp"
if [ "${TEST}" -eq 1 ]; then
    # Build the package, install it, then build/run a find_package consumer.
    docker run --rm -v "${OUT}:/deb-out" "${IMAGE}:${TAG}" bash -c "
        set -euxo pipefail
        cd ${WORKDIR}
        bash tools/bump-changelog.sh
        dpkg-buildpackage -us -uc -b
        DEB=\$(ls /opt/*.deb | head -n1)
        dpkg -i \"\${DEB}\"
        mkdir -p /tmp/consumer && cd /tmp/consumer
        cat > main.cpp <<'EOF'
#include <marinholab/solvers/osqp.h>
#include <iostream>
namespace osqp = marinholab::solvers::osqp;
int main()
{
    osqp::Configuration config;
    config.set(\"eps_abs\", 1.0e-9);
    osqp::Solver solver(config);
    Eigen::MatrixXd H = Eigen::MatrixXd::Identity(2, 2);
    Eigen::VectorXd f(2); f << -1.0, -1.0;
    Eigen::MatrixXd A(1, 2); A << 1.0, 0.0;
    Eigen::VectorXd b(1); b << 0.2;
    Eigen::MatrixXd Aeq = Eigen::MatrixXd::Zero(1, 2);
    Eigen::VectorXd beq = Eigen::VectorXd::Zero(1);
    Eigen::VectorXd x = solver.solve_quadratic_program(H, f, A, b, Aeq, beq);
    std::cout << \"consumer x = \" << x.transpose() << std::endl;
    return 0;
}
EOF
        cat > CMakeLists.txt <<'EOF'
cmake_minimum_required(VERSION 3.16)
project(consumer CXX)
find_package(marinholab_solver_osqp REQUIRED)
add_executable(consumer main.cpp)
target_link_libraries(consumer PRIVATE marinholab::solvers::osqp)
EOF
        cmake -B b -DCMAKE_BUILD_TYPE=Release . && cmake --build b && ./b/consumer
        cp \"\${DEB}\" /deb-out/
    "
else
    docker run --rm -v "${OUT}:/deb-out" "${IMAGE}:${TAG}" bash -c "
        set -euxo pipefail
        cd ${WORKDIR}
        bash tools/bump-changelog.sh
        dpkg-buildpackage -us -uc -b
        cp /opt/*.deb /deb-out/
    "
fi

echo "=== .deb written to ${OUT} ==="
ls -la "${OUT}"
