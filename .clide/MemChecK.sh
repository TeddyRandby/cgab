#!/usr/bin/env sh

clide b -t=debug

cd "$CLIDE_PATH/../" || exit

exec valgrind -s --track-origins=yes --leak-check=full ./build/gab test/mod.gab
