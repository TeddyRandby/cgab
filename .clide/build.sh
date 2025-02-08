#!/usr/bin/env bash

cd "$CLIDE_PATH/../" || exit 1

if ! test -e "configuration"; then
  clide configure || exit 1
fi

export GAB_BUILDTYPE=
export GAB_CCFLAGS=
export GAB_TARGETS=
source configuration || exit 1

export unixflags="-DGAB_PLATFORM_UNIX -D_POSIX_C_SOURCE=200809L"
export winflags="-DGAB_PLATFORM_WIN"

function build {
  echo "Building $1"

  flags="-std=c23 -fPIC -Wall --target=$1 -o build-$1/gab -Iinclude -Ivendor $GAB_CCFLAGS"

  platform=""

  if [[ "$1" =~ "linux" ]]; then
    platform="$unixflags"
  elif [[ "$1" =~ "mac" ]]; then
    platform="$unixflags"
  elif [[ "$1" =~ "windows" ]]; then
    platform="$winflags"
  fi

  echo "   $flags $platform"

  zig cc $flags $platform src/**/*.c || exit 1

  echo "Success!"
}
export -f build

echo "Beginning compilation."
echo "$GAB_TARGETS"

echo "$GAB_TARGETS" | tr ' ' '\n' | parallel mkdir -p "build-{}" '&&' build "{}" '||' exit 1 '&&' echo "Built {}"
