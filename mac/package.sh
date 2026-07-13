#!/usr/bin/env bash
#
# Builds DemoInk in Release and packages it into a distributable .dmg with a
# drag-to-Applications layout. The macOS equivalent of the Windows
# `installer/DemoInk.iss` step (Inno Setup).
#
# Usage:  mac/package.sh
# Output: mac/dist/DemoInk-<version>.dmg  (plus the standalone .app alongside)
#
# No signing/notarization (per docs/mac-port.md — personal use, no Apple
# Developer account). Gatekeeper will show the "unidentified developer" wall;
# first launch is right-click → Open. See the README note printed at the end.

set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
PROJECT_DIR="$SCRIPT_DIR/DemoInk"
PROJECT="$PROJECT_DIR/DemoInk.xcodeproj"
DIST_DIR="$SCRIPT_DIR/dist"
BUILD_DIR="$DIST_DIR/build"
APP_NAME="DemoInk"

echo "==> Building $APP_NAME (Release)…"
xcodebuild \
    -project "$PROJECT" \
    -scheme "$APP_NAME" \
    -configuration Release \
    -derivedDataPath "$BUILD_DIR" \
    CODE_SIGN_IDENTITY="-" \
    build >/dev/null

APP_PATH="$BUILD_DIR/Build/Products/Release/$APP_NAME.app"
if [[ ! -d "$APP_PATH" ]]; then
    echo "!! Build succeeded but $APP_PATH is missing." >&2
    exit 1
fi

# Read the version straight from the built app's Info.plist so the .dmg name
# always matches what shipped.
VERSION="$(defaults read "$APP_PATH/Contents/Info" CFBundleShortVersionString 2>/dev/null || echo "0.0")"
echo "==> Packaged version: $VERSION"

# Stage a clean folder holding the .app + an /Applications symlink so the .dmg
# opens to the familiar drag-to-install layout.
STAGE="$DIST_DIR/stage"
rm -rf "$STAGE"
mkdir -p "$STAGE"
cp -R "$APP_PATH" "$STAGE/"
ln -s /Applications "$STAGE/Applications"

DMG_PATH="$DIST_DIR/$APP_NAME-$VERSION.dmg"
rm -f "$DMG_PATH"

echo "==> Building ${DMG_PATH}…"
hdiutil create \
    -volname "$APP_NAME $VERSION" \
    -srcfolder "$STAGE" \
    -ov -format UDZO \
    "$DMG_PATH" >/dev/null

# Also drop the raw .app next to the .dmg for the "portable" artifact, mirroring
# the Windows two-artifact release rule (portable exe + installer).
rm -rf "$DIST_DIR/$APP_NAME.app"
cp -R "$APP_PATH" "$DIST_DIR/"

rm -rf "$STAGE"

echo ""
echo "✅ Done."
echo "   DMG : $DMG_PATH"
echo "   App : $DIST_DIR/$APP_NAME.app"
echo ""
echo "First launch on another Mac: right-click the app → Open (unsigned build,"
echo "Gatekeeper shows an 'unidentified developer' warning the first time only)."
