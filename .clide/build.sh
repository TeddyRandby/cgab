#!/usr/bin/env bash

cd "$CLIDE_PATH/../" || exit 1

echo "Beginning compilation."

if ! test -e "configuration"; then
  clide configure || exit 1
fi

export GAB_BUILDTYPE=
export GAB_TARGETS=
source configuration || exit 1

echo $GAB_TARGETS | tr ' ' '\n' | parallel mkdir -p "build-{}" '&&' make TARGET="{}" '||' exit 1 '&&' echo "Built {}"

##while read target || [[ -n $target ]]
#do
#  mkdir -p "build-$target" # Make the build folder if it doesn't exist
#  echo "Building target $target..."
#  make TARGET="$target" || exit 1
#  echo "Done."
#done

echo "Success! "
