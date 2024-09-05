#!/usr/bin/bash

hyperfine -w 8 -r 30 'gab run fib' 'python3 fib.py' 'lua fib.lua' 'ruby fib.rb'
