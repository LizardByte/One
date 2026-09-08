#!/usr/bin/env bash
set -euo pipefail
export PATH="/usr/bin:$PATH"

# Invoked by ExternalProject in its build directory. Never writes to Sunshine
# or to the system dependency prefix.
mode=$1
source_dir=$(cygpath -u "$2")
install_dir=$(cygpath -u "$3")
compiler=$(cygpath -u "$4")
jobs=$5
export PATH="$(dirname "$compiler"):/usr/bin:$PATH"

case "$mode" in
    configure)
        exec "$source_dir/configure" \
            --prefix="$install_dir" --cc="$compiler" --target-os=mingw32 \
            --disable-everything --disable-autodetect --disable-programs \
            --disable-doc --disable-debug --disable-network --disable-shared \
            --disable-avdevice --disable-avformat --disable-avfilter --disable-swresample \
            --enable-static --enable-avcodec --enable-avutil --enable-swscale \
            --enable-decoder=h264,hevc,libdav1d --enable-parser=h264,hevc,av1 \
            --enable-libdav1d --pkg-config-flags=--static --extra-ldflags=-static
        ;;
    build) exec make -j "$jobs" ;;
    install) exec make install ;;
    *) exit 2 ;;
esac
