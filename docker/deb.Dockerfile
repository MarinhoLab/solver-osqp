FROM ubuntu:noble
LABEL authors="murilomarinho"
SHELL ["/bin/bash", "-c"]
ENV DEBIAN_FRONTEND=noninteractive

# Toolchain, C++ dependency and Debian packaging tools. OSQP and its
# linear-solver (qdldl) are vendored as git submodules, so only Eigen needs
# to come from the archive.
RUN apt-get update && apt-get install -y --no-install-recommends \
        build-essential g++ cmake make pkg-config git \
        libeigen3-dev \
        dpkg-dev debhelper devscripts \
    && rm -rf /var/lib/apt/lists/*

WORKDIR /opt/solver-osqp
COPY . /opt/solver-osqp

CMD ["bash", "/opt/solver-osqp/docker/build-deb.sh"]
