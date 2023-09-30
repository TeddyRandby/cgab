project('Gab Scripting Language',
  'c',
  default_options: ['c_std=c2x']
)

sources = files()
subdir('src/gab')
subdir('src/os')
subdir('src/cli')

nc = dependency('ncursesw')
http = dependency('libllhttp')
uri = dependency('liburiparser')

cgab = shared_library('cgab',
  sources,
  install: true,
)

cgab_debug = shared_library('cgab_debug',
  sources,
  c_args: [ '-DDEBUG=1' ],
)

shared_library('cgaburi-parser',
  'lib/uri-parser.c',
  dependencies: [uri],
  link_with: [cgab],
  install_dir: '/usr/local/lib/gab/',
  install: true,
)

shared_library('cgabhttp-parser',
  'lib/http-parser.c',
  dependencies: [http],
  link_with: [cgab],
  install_dir: '/usr/local/lib/gab/',
  install: true,
)

shared_library('cgabfiber',
  'lib/fiber.c',
  link_with: [cgab],
  install_dir: '/usr/local/lib/gab/',
  install: true,
)

shared_library('cgabterm',
  'lib/term.c',
  dependencies: [nc],
  link_with: [cgab],
  install_dir: '/usr/local/lib/gab/',
  install: true,
)

shared_library('cgabsocket',
  'lib/socket.c',
  link_with: [cgab],
  install_dir: '/usr/local/lib/gab/',
  install: true,
)

shared_library('cgabtime',
  'lib/time.c',
  link_with: [cgab],
  install_dir: '/usr/local/lib/gab/',
  install: true,
)

shared_library('cgabmap',
  'lib/map.c',
  link_with: [cgab],
  install_dir: '/usr/local/lib/gab/',
  install: true,
)

shared_library('cgablist',
  'lib/list.c',
  link_with: [cgab],
  install_dir: '/usr/local/lib/gab/',
  install: true,
)

shared_library('cgabsymbol',
  'lib/symbol.c',
  link_with: [cgab],
  install_dir: '/usr/local/lib/gab/',
  install: true,
)

shared_library('cgabstring',
  'lib/string.c',
  link_with: [cgab],
  install_dir: '/usr/local/lib/gab/',
  install: true,
)

shared_library('cgabrecord',
  'lib/rec.c',
  'lib/map.h',
  'lib/list.h',
  link_with: [cgab],
  install_dir: '/usr/local/lib/gab/',
  install: true,
)

shared_library('cgabnum',
  'lib/num.c',
  link_with: [cgab],
  install_dir: '/usr/local/lib/gab/',
  install: true,
)

shared_library('cgabio',
  'lib/io.c',
  link_with: [cgab],
  install_dir: '/usr/local/lib/gab/',
  install: true,
)

shared_library('cgabdis',
  'lib/dis.c',
  link_with: [cgab],
  install_dir: '/usr/local/lib/gab/',
  install: true,
)

# shared_library('cgabpry',
#   'src/lib/pry.c',
#   link_with: [cgab],
#   install_dir: '/usr/local/lib/gab/',
#   install: true,
# )

install_headers(
  'include/bytecode.h',
  'include/gab.h',
  'include/core.h',
  'include/status_code.h',
  'include/types.h',
  'include/slice.h',
  'include/array.h',
  'include/vector.h',
  'include/dict.h',
  'include/hash.h',
  'include/gc.h',
  install_dir: '/usr/local/include/gab/'
  )

install_data(
  'std/String.gab',
  'std/Number.gab',
  'std/Time.gab',
  'std/List.gab',
  'std/Collection.gab',
  'std/Map.gab',
  'std/Pry.gab',
  'std/Test.gab',
  'std/Record.gab',
  'std/HTTP.gab',
  install_dir: '/usr/local/share/gab/std/'
)

executable('gab',
  'src/cli/main.c',
  link_with: [cgab],
  install: true,
)
