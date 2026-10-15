#!/usr/bin/env bash
# Check a downstream CMake project against an INSTALLED
# libmarinholab-solver-osqp: find_package() + the imported target only, then
# solve a few QPs and verify the numbers.
#
# The consumer lives in docker/consumer/ and is used by both this script and
# docker/build-deb.sh --test, so "installed from the repository" and "installed
# from the locally built .deb" are tested with exactly the same code.
#
# Usage:
#   bash docker/test-ppa.sh             # apt install from the PPA (amd64 + arm64)
#   bash docker/test-ppa.sh --native    # current architecture only
set -euo pipefail

ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
IMAGE="marinholab-solver-osqp-ppa"
TAG="$(date +%Y%m%d%H%M)"

ARCHES=(linux/amd64 linux/arm64)
[ "${1:-}" = "--native" ] && ARCHES=()

if [ "${#ARCHES[@]}" -eq 0 ]; then
    echo "=== Building ${IMAGE}:${TAG} (native architecture) ==="
    docker build -f "${ROOT}/docker/ppa.Dockerfile" -t "${IMAGE}:${TAG}" "${ROOT}"
    echo "=== Running the consumer ==="
    docker run --rm "${IMAGE}:${TAG}"
else
    for PLATFORM in "${ARCHES[@]}"; do
        echo "=== Building ${IMAGE}:${TAG} for ${PLATFORM} ==="
        docker build --platform "${PLATFORM}" -f "${ROOT}/docker/ppa.Dockerfile" \
            -t "${IMAGE}:${TAG}" "${ROOT}"
        echo "=== Running the consumer on ${PLATFORM} ==="
        docker run --rm --platform "${PLATFORM}" "${IMAGE}:${TAG}"
    done
fi

echo "=== PPA consumer test passed ==="
