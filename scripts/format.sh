#!/usr/bin/env bash
set -e

echo "Formatting source files..."
find src include -name '*.m' -o -name '*.h' | xargs clang-format -i
echo "✓ Formatted source files"