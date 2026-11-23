#!/bin/sh
#---------------------------------------------------------------------------------
# Assemble a ready-to-copy SD card directory for Syobon Action U.
#
# Result:
#   dist/wiiu/apps/SyobonActionU/SyobonActionU.wuhb
#   dist/wiiu/apps/SyobonActionU/SyobonActionU.rpx   (+ resources for .rpx use)
#
# Copy the "wiiu" folder to the root of your SD card.
#---------------------------------------------------------------------------------
set -e
HERE=$(cd "$(dirname "$0")" && pwd)
DEST="$HERE/dist/wiiu/apps/SyobonActionU"

if [ ! -f "$HERE/SyobonActionU.wuhb" ]; then
    echo "SyobonActionU.wuhb not found - run ./build.sh first." >&2
    exit 1
fi

rm -rf "$HERE/dist"
mkdir -p "$DEST"

cp "$HERE/SyobonActionU.wuhb" "$DEST/"
cp "$HERE/SyobonActionU.rpx"  "$DEST/"

# Resources for launching the loose .rpx (also embedded in the .wuhb).
cp -r "$HERE/BGM" "$HERE/SE" "$HERE/res" "$DEST/"

echo "Packaged to: $DEST"
find "$HERE/dist" -type f | sed "s|$HERE/dist/||" | sort | head -50
