FROM ubuntu:noble
LABEL authors="murilomarinho"
SHELL ["/bin/bash", "-c"]
ENV DEBIAN_FRONTEND=noninteractive

# Start from a bare Ubuntu: what makes this a repository test is that nothing
# from the source tree is present, only what apt installs.
#
# The package itself depends on nothing but libc6/libgcc-s1/libstdc++6; the
# toolchain, CMake and Eigen are here because a consumer also has to *build*
# against it (Eigen's headers are needed by the public header's API).
RUN apt-get update && apt-get install -y --no-install-recommends \
        build-essential g++ cmake libeigen3-dev \
        curl ca-certificates gnupg \
    && rm -rf /var/lib/apt/lists/*

# The APT repository the package is published to. A bare image needs gnupg to
# dearmor the signing key (apt cannot read the armored KEY.gpg directly).
# Override for a different repository with
#   --build-arg APT_REPO=... --build-arg APT_KEYRING=...
ARG APT_REPO=https://smartarmstack.github.io/smart_arm_stack_ROS2
ARG APT_KEYRING=smartarmstack_lgpl
RUN curl -s --compressed "${APT_REPO}/KEY.gpg" \
        | gpg --dearmor \
        | tee "/etc/apt/trusted.gpg.d/${APT_KEYRING}.gpg" >/dev/null \
    && curl -s --compressed -o "/etc/apt/sources.list.d/${APT_KEYRING}.list" \
        "${APT_REPO}/${APT_KEYRING}.list"

ARG PACKAGE=libmarinholab-solver-osqp
RUN apt-get update && apt-get install -y "${PACKAGE}" \
    && apt-get clean && rm -rf /var/lib/apt/lists/* \
    && echo "=== installed files ===" && dpkg -L "${PACKAGE}" | grep -E 'cmake|\.so|\.h$'

# Downstream CMake project: find_package() + the imported target only.
COPY docker/consumer /opt/consumer
RUN cmake -S /opt/consumer -B /opt/consumer/build \
    && cmake --build /opt/consumer/build

# Prints the solutions and exits non-zero if any of them is wrong.
CMD ["/opt/consumer/build/consumer"]
