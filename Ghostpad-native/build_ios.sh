#!/bin/bash
# build_ios.sh — Build, sign, and deploy Ghostpad to an iPad
# Usage: ./build_ios.sh [--deploy] [--device DEVICE_ID] [--team TEAM_ID]
set -e

SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
BUILD_DIR="$SCRIPT_DIR/build_ios"
TEAM_ID="${TEAM_ID:-LR6H5GZD2U}"
DEVICE_ID="${DEVICE_ID:-00008132-000161462E45001C}"
DEPLOY=0

while [[ $# -gt 0 ]]; do
    case "$1" in
        --deploy) DEPLOY=1; shift ;;
        --device) DEVICE_ID="$2"; shift 2 ;;
        --team) TEAM_ID="$2"; shift 2 ;;
        *) echo "Unknown option: $1"; exit 1 ;;
    esac
done

echo "=============================================="
echo " Ghostpad iOS Build"
echo " Team: $TEAM_ID"
echo " Device: $DEVICE_ID"
echo " Deploy: $([ $DEPLOY -eq 1 ] && echo 'YES' || echo 'NO')"
echo "=============================================="

# ── 1. Clean up extended attributes and .DS_Store ──
echo ""
echo "[1/5] Cleaning extended attributes..."
find "$SCRIPT_DIR" -name ".DS_Store" -delete 2>/dev/null || true
find "$BUILD_DIR" -name ".DS_Store" -delete 2>/dev/null || true
xattr -cr "$SCRIPT_DIR" 2>/dev/null || true
xattr -cr "$BUILD_DIR" 2>/dev/null || true

# ── 2. CMake configure ──
echo ""
echo "[2/5] Configuring with CMake..."
mkdir -p "$BUILD_DIR"
cmake -S "$SCRIPT_DIR" -B "$BUILD_DIR" \
    -G Xcode \
    -DCMAKE_SYSTEM_NAME=iOS \
    -DCMAKE_OSX_ARCHITECTURES=arm64 \
    -DCMAKE_OSX_DEPLOYMENT_TARGET=14.0 \
    -DIOS=ON \
    -DIOS_SIGNING_TEAM="$TEAM_ID"

# ── 3. xcodebuild (let it fail at CodeSign — that's expected) ──
echo ""
echo "[3/5] Building with xcodebuild..."
xcodebuild -project "$BUILD_DIR/GhostpadNative.xcodeproj" \
    -target ghostpad-native \
    -sdk iphoneos \
    -configuration Debug \
    DEVELOPMENT_TEAM="$TEAM_ID" \
    CODE_SIGN_STYLE=Automatic \
    COPY_PHASE_STRIP=YES \
    -allowProvisioningUpdates \
    CODE_SIGNING_ALLOWED=NO \
    -quiet 2>&1 || echo "Build phase complete (code sign expected to fail)"

# ── 4. Clean and re-sign the .app ──
APP_PATH="$BUILD_DIR/Debug-iphoneos/ghostpad-native.app"
XCCENT="$BUILD_DIR/build/ghostpad-native.build/Debug-iphoneos/ghostpad-native.app.xcent"

if [ ! -d "$APP_PATH" ]; then
    echo "ERROR: .app not found at $APP_PATH"
    exit 1
fi

echo ""
echo "[4/5] Cleaning and signing with ditto + codesign..."

# Use ditto to strip all resource forks/Finder metadata
TMP_APP="/tmp/ghostpad-clean.app"
rm -rf "$TMP_APP"
ditto --norsrc "$APP_PATH" "$TMP_APP"

# Find signing identity (use the one matching the team)
SIGN_ID=$(security find-identity -v -p codesigning 2>/dev/null | grep "($TEAM_ID)" | head -1 | awk '{print $2}')
if [ -z "$SIGN_ID" ]; then
    echo "ERROR: No signing identity found for team $TEAM_ID"
    exit 1
fi

# Sign
/usr/bin/codesign --force --sign "$SIGN_ID" \
    ${XCCENT:+--entitlements "$XCCENT"} \
    --timestamp=none \
    --generate-entitlement-der \
    "$TMP_APP"

# Replace original
rm -rf "$APP_PATH"
cp -r "$TMP_APP" "$APP_PATH"
rm -rf "$TMP_APP"

echo "   Signing complete."

# ── 5. Deploy (optional) ──
if [ $DEPLOY -eq 1 ]; then
    echo ""
    echo "[5/5] Deploying to device $DEVICE_ID..."
    xcrun devicectl device install app --device "$DEVICE_ID" "$APP_PATH"
    echo "   App installed on iPad!"
else
    echo ""
    echo "[5/5] Skipping deploy (use --deploy to install on iPad)"
fi

echo ""
echo "=============================================="
echo " Build complete!"
echo " App: $APP_PATH"
echo "=============================================="
