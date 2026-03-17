#!/usr/bin/env bash
#
# build.sh — Build (and optionally flash) pico2-ili9341 via Docker
#
# Usage:
#   ./build.sh              # build only
#   ./build.sh flash        # build + copy .uf2 to mounted Pico
#   ./build.sh clean        # remove build directory
#

set -euo pipefail

IMAGE_NAME="pico2-ili9341-build"
BUILD_DIR="build"
TARGET="pico2_ili9341"
SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"

# Default Pico mount — override: PICO_MOUNT=/media/$USER/RPI-RP2 ./build.sh flash
PICO_MOUNT="${PICO_MOUNT:-/Volumes/RP2350}"

cmd="${1:-build}"

case "$cmd" in
    clean)
        echo ">>> Cleaning build directory"
        rm -rf "${SCRIPT_DIR}/${BUILD_DIR}"
        echo ">>> Done"
        ;;

    build|flash)
        # ---- Build Docker image (cached unless Dockerfile changes) ----
        echo ">>> Building Docker image: ${IMAGE_NAME}"
        docker build -t "${IMAGE_NAME}" "${SCRIPT_DIR}"

        # ---- Run build inside container ----
        echo ">>> Compiling inside container"
        docker run --rm \
            -v "${SCRIPT_DIR}":/project \
            "${IMAGE_NAME}" \
            bash -c "
                mkdir -p ${BUILD_DIR} && \
                cd ${BUILD_DIR} && \
                cmake -DPICO_BOARD=pico2 -DPICO_PLATFORM=rp2350-arm-s .. && \
                make -j\$(nproc)
            "

        UF2="${SCRIPT_DIR}/${BUILD_DIR}/src/${TARGET}.uf2"
        echo ""
        echo ">>> UF2 ready: ${UF2}"

        # ---- Flash if requested ----
        if [ "$cmd" = "flash" ]; then
            if [ ! -d "${PICO_MOUNT}" ]; then
                echo "ERROR: Pico not mounted at ${PICO_MOUNT}"
                echo "       Hold BOOTSEL, plug in, then retry."
                echo "       Or override: PICO_MOUNT=/path ./build.sh flash"
                exit 1
            fi
            cp "${UF2}" "${PICO_MOUNT}/"
            echo ">>> Flashed to ${PICO_MOUNT}"
        fi
        ;;

    *)
        echo "Usage: $0 [build|flash|clean]"
        exit 1
        ;;
esac
