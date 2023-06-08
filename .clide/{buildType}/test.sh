#!/usr/bin/env sh

clide build build -t="$buildtype"

cd "$CLIDE_PATH/../" || exit

exec ./build/gab test/mod.gab
