#!/usr/bin/env sh
set -eu

if [ -z "${VITASDK:-}" ]; then
    echo "ERROR: VITASDK is not set." >&2
    exit 1
fi

if ! command -v arm-vita-eabi-gcc >/dev/null 2>&1; then
    echo "ERROR: arm-vita-eabi-gcc not found in PATH." >&2
    exit 1
fi

cmake -S . -B build \
    -DCMAKE_TOOLCHAIN_FILE="$VITASDK/share/vita.toolchain.cmake" \
    -DCMAKE_BUILD_TYPE=Release

cmake --build build --parallel

echo
echo "Build output:"
find build -maxdepth 1 \( -name '*.vpk' -o -name 'eboot.bin' \) -print
