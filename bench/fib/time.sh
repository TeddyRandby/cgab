#!/usr/sbin/fish

hyperfine -w 5 -r 10 'gab fib.gab' 'python3 fib.py' 'lua fib.lua' 'node fib.js' 'ruby fib.rb'
