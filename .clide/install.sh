#!/usr/bin/env bash

cd "$CLIDE_PATH/../" || exit 1

if ! test -e "build/configuration"; then
  clide configure || exit 1
fi


export GAB_PREFIX=
export GAB_INSTALLPREFIX=
export GAB_BUILDTYPE=
source build/configuration || exit 1

clide build || exit 1

echo "Beginning installation."

make install || exit 1

echo "Success!"
