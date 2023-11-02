#!/usr/bin/env sh

cd "$CLIDE_PATH/../" || exit

meson setup --reconfigure -Dprefix="$installprefix" -Dbuildtype="$buildtype" build

echo "Success! Project configured."
