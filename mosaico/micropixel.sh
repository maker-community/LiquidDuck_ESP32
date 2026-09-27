#!/bin/sh
set -eu
: "${MICROPIXEL_SDK_DIR:?Set MICROPIXEL_SDK_DIR to the MicroPixel SDK root}"
: "${WASI_SDK_PATH:?Set WASI_SDK_PATH to the WASI SDK root}"
: "${WAMRC:?Set WAMRC to the RISC-V capable wamrc executable}"
exec "${PYTHON:-python3}" "$MICROPIXEL_SDK_DIR/micropixel" "$@"
