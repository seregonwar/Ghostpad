#!/bin/bash
# Ghostpad - WSL dependency installer
# Run inside Ubuntu 22.04 via WSL. Do not run on Windows directly.
set -e

echo "[Deps] Updating base packages..."
sudo apt-get update -qq
sudo apt-get install -y -qq wget gnupg lsb-release ca-certificates

# clang-18 / lld-18 are NOT in Ubuntu 22.04 default repos.
# They live on apt.llvm.org. Add the repo if clang-18 isn't already installed.
if ! command -v clang-18 &>/dev/null; then
    echo "[Deps] Adding LLVM 18 apt repository..."
    CODENAME=$(lsb_release -cs)
    wget -qO /tmp/llvm-snapshot.gpg.key https://apt.llvm.org/llvm-snapshot.gpg.key
    sudo cp /tmp/llvm-snapshot.gpg.key /etc/apt/trusted.gpg.d/apt.llvm.org.asc
    echo "deb http://apt.llvm.org/${CODENAME}/ llvm-toolchain-${CODENAME}-18 main" \
        | sudo tee /etc/apt/sources.list.d/llvm-18.list
    sudo apt-get update -qq
    echo "[Deps] Installing clang-18 and lld-18..."
    sudo apt-get install -y clang-18 lld-18
else
    echo "[Deps] clang-18 already installed, skipping LLVM repo."
fi

echo "[Deps] Installing remaining build tools..."
sudo apt-get install -y socat cmake meson pkg-config unzip python3 python3-pyelftools

echo "[Deps] All dependencies installed successfully."
clang-18 --version | head -1
