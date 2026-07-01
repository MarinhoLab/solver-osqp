FROM ubuntu:noble
LABEL authors="murilomarinho"
SHELL ["/bin/bash", "-c"]
ENV BASH_ENV="/etc/bash_env"

RUN apt-get update
RUN apt-get install -y python3 python3-pip cmake libeigen3-dev git
RUN pip install setuptools-git-versioning numpy --break-system-packages
COPY . /root/solver-osqp
