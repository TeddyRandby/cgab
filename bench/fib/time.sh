#!/usr/bin/bash

hyperfine -w 8 -r 30 'python3 fib.py' 'gab fib.gab' 'lua fib.lua' 'ruby fib.rb' 'gab fib_block.gab'
