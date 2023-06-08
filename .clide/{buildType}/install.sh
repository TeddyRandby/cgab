#!/usr/bin/env sh

cd "$CLIDE_PATH/../" || exit

meson setup --reconfigure build --buildtype="$buildtype"

cd ./build && sudo ninja install
