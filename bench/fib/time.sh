#!/usr/bin/bash

hyperfine -w 8 -r 30 'gab fib_block.gab' 'gab fib.gab' 'gab fib_split.gab' 'python3 fib.py' 'lua fib.lua' 'ruby fib.rb'
