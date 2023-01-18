#!/usr/bin/fish

hyperfine -w 5 -r 10 "gab ./prop.gab" "python ./prop.py" "ruby ./prop.rb" "lua prop.lua"
