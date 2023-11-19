#!/usr/bin/env bash

cd "$CLIDE_PATH/../" || exit 1

echo "Beginning compilation."

if ! test -e "build/configuration"; then
  clide configure || exit 1
fi

export GAB_PREFIX=
export GAB_INSTALLPREFIX=
export GAB_BUILDTYPE=
source build/configuration || exit 1

make || exit 1

echo "Success! "
