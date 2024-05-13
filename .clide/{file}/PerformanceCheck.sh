#!/usr/bin/env bash

cd "$CLIDE_PATH/../" || exit 1

valgrind --tool=callgrind --callgrind-out-file=callgrind.out --dump-line=yes gab run "$file"

~/pyenv/bin/gprof2dot --format=callgrind callgrind.out > dot.out

dot -Tpng dot.out -o graph.out.png

feh graph.out.png

rm *.out*
