#!/usr/bin/env bash

set -e

BINARY="build/dwin"

if [ ! -f "$BINARY" ]; then
  echo "Error: $BINARY not found. Run 'make release' first."
  exit 1
fi

echo "Self-signing $BINARY..."
codesign --force --deep --sign - "$BINARY"
echo "✓ Self-signed $BINARY"
