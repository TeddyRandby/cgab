#!/usr/sbin/fish

hyperfine -w 2 -r 20 'gab ./fib.gab' 'python3 fib.py' 'luajit -joff fib.lua' 'node fib.js' 'ruby fib.rb'
