#! /usr/bin/fish

valgrind --tool=callgrind --callgrind-out-file=callgrind.out --dump-line=yes gab mat.gab

# gprof2dot --format=callgrind callgrind.out > dot.out
#
# dot -Tpng dot.out -o graph.out.png
#
# feh graph.out.png
#
# rm *.out*
