ARG DOCKER_BASE

FROM $DOCKER_BASE

LABEL maintainer="PiP - Process in Process <procinproc-info@googlegroups.com>"

ARG PIP_VERSION
ARG DISTRO
ARG GITHUB_REPOSITORY
ARG PIP_BUILD_TOKEN

WORKDIR /tmp/work

COPY install.sh ./

RUN sh -x ./install.sh
