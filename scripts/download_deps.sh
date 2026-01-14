#!/bin/bash
# Download third-party dependencies

set -e

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
PROJECT_DIR="$(dirname "$SCRIPT_DIR")"
THIRD_PARTY_DIR="$PROJECT_DIR/third_party"

echo "Downloading dependencies to $THIRD_PARTY_DIR"

# miniaudio
echo "Downloading miniaudio..."
mkdir -p "$THIRD_PARTY_DIR/miniaudio"
curl -sL -o "$THIRD_PARTY_DIR/miniaudio/miniaudio.h" \
    "https://raw.githubusercontent.com/mackron/miniaudio/master/miniaudio.h"
echo "  -> miniaudio.h downloaded"

# RNNoise (optional)
if [ "${DOWNLOAD_RNNOISE:-0}" = "1" ]; then
    echo "Cloning RNNoise..."
    if [ ! -d "$THIRD_PARTY_DIR/rnnoise" ]; then
        git clone --depth 1 https://github.com/xiph/rnnoise.git "$THIRD_PARTY_DIR/rnnoise"
    fi
    echo "  -> RNNoise cloned"
fi

echo "Done!"
