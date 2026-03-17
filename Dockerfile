FROM ubuntu:24.04 AS build-env

ARG DEBIAN_FRONTEND=noninteractive
ARG PICO_SDK_VERSION=2.1.1

RUN apt-get update && apt-get install -y --no-install-recommends \
    build-essential \
    cmake \
    gcc-arm-none-eabi \
    libnewlib-arm-none-eabi \
    libstdc++-arm-none-eabi-newlib \
    git \
    python3 \
    ca-certificates \
    && rm -rf /var/lib/apt/lists/*

# Clone SDK + submodules (tinyusb, etc.)
RUN git clone --branch ${PICO_SDK_VERSION} --depth 1 \
    https://github.com/raspberrypi/pico-sdk.git /opt/pico-sdk && \
    cd /opt/pico-sdk && git submodule update --init --depth 1

ENV PICO_SDK_PATH=/opt/pico-sdk

WORKDIR /project
