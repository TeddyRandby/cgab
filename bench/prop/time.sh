#!/usr/bin/fish

hyperfine "gab ./prop.gab" "python ./prop.py" "node ./prop.js"
