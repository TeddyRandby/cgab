project('Gab Scripting Language',
  'c',
  default_options: 'default_library=static'
)

subdir('src/gab')
subdir('src/core')
subdir('src/cli')

dl = dependency('dl')

libcgab = library('cgab',
  sources,
  dependencies: [ dl ],
)

shared_library('cgabsockets',
  'src/lib/socket.c',
  link_with: libcgab,
  install: true,
)

shared_library('cgabregex',
  'src/lib/regex.c',
  link_with: libcgab,
  install: true,
)

shared_library('cgabmath',
  'src/lib/math.c',
  link_with: libcgab,
  install: true,
)

shared_library('cgabtime',
  'src/lib/time.c',
  link_with: libcgab,
  install: true,
)
shared_library('cgabobject',
  'src/lib/object.c',
  link_with: libcgab,
  install: true,
)

executable('gab',
  'src/cli/main.c',
  dependencies: [ dl ],
  link_with: libcgab,
  install: true,
)
