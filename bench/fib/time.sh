#!/usr/sbin/fish

hyperfine -w 8 -r 10 'python3 fib.py' 'gab fib_message.gab' 'gab fib.gab' 'lua fib.lua' 'ruby fib.rb'
