#!/usr/bin/env bash

cd "$CLIDE_PATH/../" || exit 1

perf record gab run "${file:0:-4}"

perf report

rm perf.data
