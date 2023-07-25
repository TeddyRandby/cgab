#!/usr/sbin/fish

hyperfine -w 8 -r 10 'python3 countup.py' 'gab countup.gab' 'lua countup.lua' 'ruby countup.rb'
