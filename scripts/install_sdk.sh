#!/bin/bash
# Ghostpad - PS5 SDK downloader and installer
set -e

SDK_DIR="/opt/ps5-payload-sdk"
SDK_ZIP="/tmp/ps5-payload-sdk.zip"
SDK_URL="https://github.com/ps5-payload-dev/sdk/releases/latest/download/ps5-payload-sdk.zip"
SDK_VERSION_FILE="${SDK_DIR}/.sdk_etag"

# Always check for the latest SDK version using HTTP headers
echo "[SDK] Checking for latest PS5 Payload SDK..."
REMOTE_ETAG=$(wget --spider -S "${SDK_URL}" 2>&1 | grep -i "ETag:" | tail -1 | tr -d '[:space:]' || true)
LOCAL_ETAG=""
if [ -f "${SDK_VERSION_FILE}" ]; then
    LOCAL_ETAG=$(cat "${SDK_VERSION_FILE}" 2>/dev/null || true)
fi

if [ -f "${SDK_DIR}/toolchain/prospero.mk" ] && [ -n "${REMOTE_ETAG}" ] && [ "${REMOTE_ETAG}" = "${LOCAL_ETAG}" ]; then
    echo "[SDK] Already up to date at ${SDK_DIR}, skipping download."
    exit 0
fi

if [ -f "${SDK_DIR}/toolchain/prospero.mk" ] && [ -z "${REMOTE_ETAG}" ]; then
    echo "[SDK] Could not check remote version, keeping existing install."
    echo "[SDK] Delete ${SDK_DIR} to force a re-install."
    exit 0
fi

echo "[SDK] Downloading PS5 Payload SDK from GitHub releases..."
wget --progress=bar:force -O "${SDK_ZIP}" "${SDK_URL}"

echo "[SDK] Removing old SDK..."
sudo rm -rf "${SDK_DIR}"

echo "[SDK] Extracting to /opt ..."
sudo unzip -q -o -d /opt "${SDK_ZIP}"
rm -f "${SDK_ZIP}"

# Save the ETag so we can detect when a new version is available
if [ -n "${REMOTE_ETAG}" ]; then
    echo "${REMOTE_ETAG}" | sudo tee "${SDK_VERSION_FILE}" > /dev/null
fi

if [ -f "${SDK_DIR}/toolchain/prospero.mk" ]; then
    echo "[SDK] Installed successfully at ${SDK_DIR}"
else
    echo "[SDK][ERROR] prospero.mk not found after extraction."
    echo "             Contents of /opt:"
    ls /opt
    exit 1
fi
