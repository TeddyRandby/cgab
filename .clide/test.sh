#!/usr/bin/env sh

if ! cd "$CLIDE_PATH/../build"; then
  echo "Run 'clide configure' first"
  exit 1
fi

cd "../"

clide build && exec ./build/gab test/mod.gab
