#!/usr/bin/bash

hyperfine -w 8 -r 10 'gab fortail.gab' 'gab forloop.gab'
