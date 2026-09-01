#!/usr/bin/env sh
set -eu

IMAGE="${VITASDK_IMAGE:-vitasdk/vitasdk:2026.08}"

if ! command -v docker >/dev/null 2>&1; then
    echo "ERROR: Docker is not installed." >&2
    exit 1
fi

docker pull "$IMAGE"

docker run --rm \
    -v "$PWD:/workspace" \
    -w /workspace \
    "$IMAGE" \
    sh -lc '
        set -eu

        echo "VITASDK=$VITASDK"
        arm-vita-eabi-gcc --version

        if command -v vdpm >/dev/null 2>&1; then
            vdpm install libvita2d freetype libpng libjpeg-turbo zlib
        fi

        rm -rf build

        cmake -S . -B build \
            -DCMAKE_TOOLCHAIN_FILE="$VITASDK/share/vita.toolchain.cmake" \
            -DCMAKE_BUILD_TYPE=Release

        cmake --build build --parallel 2

        test -f build/spotify-vita.vpk
        test -f build/eboot.bin
    '

echo
echo "Built:"
ls -lh build/spotify-vita.vpk build/eboot.bin
