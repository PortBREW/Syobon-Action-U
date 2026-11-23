#!/bin/sh
#---------------------------------------------------------------------------------
# Build Syobon Action U (.rpx + .wuhb) inside the devkitPro Docker container.
#
# Usage:  ./build.sh
#
# The script uses your existing devkitpro/devkitppc image; nothing is
# installed on the host.
#---------------------------------------------------------------------------------
set -e

HERE=$(cd "$(dirname "$0")" && pwd)
IMAGE=${DKP_IMAGE:-devkitpro/devkitppc:latest}

echo "Building Syobon Action U with $IMAGE ..."

docker run --rm -v "$HERE":/src -w /src "$IMAGE" make "$@"

echo
echo "Artifacts:"
ls -la "$HERE"/SyobonActionU.rpx "$HERE"/SyobonActionU.wuhb
