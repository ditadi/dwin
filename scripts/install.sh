#!/usr/bin/env bash

set -e

BINARY="build/dwin"
INSTALL_PATH="/Applications/dwin.app/Contents/MacOS"

if [ ! -f "$BINARY" ]; then
    echo "Error: $BINARY not found. Run 'make codesign' first."
    exit 1
fi

echo "Installing to $INSTALL_PATH..."
mkdir -p "$INSTALL_PATH"
cp "$BINARY" "$INSTALL_PATH/dwin"
echo "✓ Installed to $INSTALL_PATH"