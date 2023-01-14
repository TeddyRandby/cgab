#!/usr/sbin/fish

hyperfine -w 8 -r 10 'gab fib.gab' 'python3 fib.py' 'lua fib.lua' 'node fib.js' 'ruby fib.rb'
