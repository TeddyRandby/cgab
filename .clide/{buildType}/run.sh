#!/usr/bin/env sh

clide build -t="$buildtype"

cd "$CLIDE_PATH/../" || exit

exec ./build/gab
