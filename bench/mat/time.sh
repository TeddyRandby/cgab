#!/usr/bin/bash

hyperfine -w 8 -r 10 'python3 mat.py' 'gab mat.gab' 'ruby mat.rb'
