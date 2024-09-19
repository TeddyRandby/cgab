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

# We need to preserve our install prefixes for the install here.
# The build type is not necessary - all our artifacts should have been built above.
sudo --preserve-env=GAB_INSTALLPREFIX make install_gab || exit 1
sudo --preserve-env=GAB_INSTALLPREFIX make install_dev || exit 1

make install_gab.lsp || exit 1
make install_modules || exit 1

echo "Success!"
