#!/usr/bin/env bash

echo "Beginning configuration."
echo "  buildtype      | $buildtype"
echo "  gab prefix     | $gabprefix"
echo "  install prefix | $installprefix"

cd "$CLIDE_PATH/../" || exit

mkdir -p "build" # Make the build folder if it doesn't exist

rm -f build/configuration # Remove the conf file if it exists

export cflags

# Create a small script which will export the variables we need, and then 
case "$buildtype" in
  debug)          cflags="-g -O0 -fsanitize=address" ;;
  debugoptimized) cflags="-g -O2" ;;
  release)        cflags="-O3 -DNDEBUG"    ;;
esac

echo "#!/usr/bin/env bash" >> build/configuration
echo "export GAB_CCFLAGS=\""$cflags"\"" >> build/configuration
echo "export GAB_PREFIX=\""$gabprefix"\"" >> build/configuration
echo "export GAB_INSTALLPREFIX=\""$installprefix"\"" >> build/configuration
chmod +x build/configuration

echo "Success!"
