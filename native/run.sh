#!/bin/sh
set -eu
native_directory=$(CDPATH= cd -- "$(dirname -- "$0")" && pwd)
mkdir -p "$native_directory/.build"
cd "$native_directory/.build"
qmake6 ../zephyr.pro
make -s -j"$(getconf _NPROCESSORS_ONLN)"
exec ./zephyr-native "$@"
