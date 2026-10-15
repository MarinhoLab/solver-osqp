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
    # Build the package, install it, then build/run the docker/consumer project
    # against the installed files. DUPLICATE_FIND_PACKAGE=ON resolves the
    # package a second time as well, which catches a config file that is not
    # re-entrant.
    docker run --rm -v "${OUT}:/deb-out" "${IMAGE}:${TAG}" bash -c "
        set -euxo pipefail
        cd ${WORKDIR}
        bash tools/bump-changelog.sh
        dpkg-buildpackage -us -uc -b
        DEB=\$(ls /opt/*.deb | head -n1)
        dpkg -i \"\${DEB}\"
        cmake -S docker/consumer -B /tmp/consumer/build -DDUPLICATE_FIND_PACKAGE=ON
        cmake --build /tmp/consumer/build
        /tmp/consumer/build/consumer
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
