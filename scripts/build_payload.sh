#!/bin/bash
# Ghostpad - Build the PS5 payload ELF
# Usage: build_payload.sh <path-to-payload-dir>
set -e

PAYLOAD_DIR="${1:?Usage: build_payload.sh <payload-dir>}"
SDK_DIR="/opt/ps5-payload-sdk"

if [ ! -f "${SDK_DIR}/toolchain/prospero.mk" ]; then
    echo "[Build][ERROR] SDK not found at ${SDK_DIR}"
    echo "               Run install_sdk.sh first."
    exit 1
fi

export PS5_PAYLOAD_SDK="${SDK_DIR}"
echo "[Build] SDK: ${PS5_PAYLOAD_SDK}"
echo "[Build] Source: ${PAYLOAD_DIR}"

make -C "${PAYLOAD_DIR}" clean all

if [ -f "${PAYLOAD_DIR}/ghostpad.elf" ]; then
    echo ""
    echo "[Build] SUCCESS: ghostpad.elf built."
    ls -lh "${PAYLOAD_DIR}/ghostpad.elf"
else
    echo "[Build][ERROR] ghostpad.elf not found after make."
    exit 1
fi
