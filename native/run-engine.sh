#!/bin/sh
set -eu
native_directory=$(CDPATH= cd -- "$(dirname -- "$0")" && pwd)
git_directory=$(git -C "$native_directory" rev-parse --path-format=absolute --git-common-dir)
export ZEPHYR_ENGINE_ROOT=${ZEPHYR_ENGINE_ROOT:-$(CDPATH= cd -- "$git_directory/.." && pwd)}
export XDG_CONFIG_HOME="$native_directory/.work/engine-config"
mkdir -p "$XDG_CONFIG_HOME"
engine_libraries="$ZEPHYR_ENGINE_ROOT/build/libs"
export ARDOUR_DATA_PATH="$ZEPHYR_ENGINE_ROOT/share:$ZEPHYR_ENGINE_ROOT/build"
export ARDOUR_CONFIG_PATH="$ZEPHYR_ENGINE_ROOT:$ZEPHYR_ENGINE_ROOT/build"
export ARDOUR_DLL_PATH="$engine_libraries"
export ARDOUR_PANNER_PATH="$engine_libraries/panners"
export ARDOUR_BACKEND_PATH="$engine_libraries/backends/dummy"
export ARDOUR_MIDIMAPS_PATH="$ZEPHYR_ENGINE_ROOT/share/midi_maps"
export ARDOUR_MIDI_PATCH_PATH="$ZEPHYR_ENGINE_ROOT/share/patchfiles"
export ARDOUR_EXPORT_FORMATS_PATH="$ZEPHYR_ENGINE_ROOT/share/export"
export VAMP_PATH="$engine_libraries/vamp-plugins:$engine_libraries/vamp-pyin"
export LD_LIBRARY_PATH="$native_directory/.work/engine-deps/usr/lib:$engine_libraries/ardour:$engine_libraries/pbd:$engine_libraries/temporal:$engine_libraries/evoral:$engine_libraries/midi++2:$engine_libraries/audiographer:$engine_libraries/ptformat:$engine_libraries/ctrl-interface/control_protocol:$engine_libraries/ctrl-interface/midi_surface:$engine_libraries/aaf:$engine_libraries/ardouralsautil${LD_LIBRARY_PATH:+:$LD_LIBRARY_PATH}"
mkdir -p "$native_directory/.build/engine"
cd "$native_directory/.build/engine"
qmake6 ../../zephyr.pro CONFIG+=engine >&2
make -s -j"$(getconf _NPROCESSORS_ONLN)" >&2
exec ./zephyr-native "$@" >&2
