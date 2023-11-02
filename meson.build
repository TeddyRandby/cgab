project('cgab',
  'c',
  default_options: ['c_std=c2x'],
  version: '0.1.0'
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

shared_library('cgaburi-parser',
  'lib/uri-parser.c',
  dependencies: [uri],
  link_with: [cgab],
  install: true,
)

shared_library('cgabhttp-parser',
  'lib/http-parser.c',
  dependencies: [http],
  link_with: [cgab],
  install: true,
)

shared_library('cgabfiber',
  'lib/fiber.c',
  link_with: [cgab],
  install: true,
)

shared_library('cgabterm',
  'lib/term.c',
  dependencies: [nc],
  link_with: [cgab],
  install: true,
)

shared_library('cgabsocket',
  'lib/socket.c',
  link_with: [cgab],
  install: true,
)

shared_library('cgabtime',
  'lib/time.c',
  link_with: [cgab],
  install: true,
)

shared_library('cgabdict',
  'lib/dict.c',
  link_with: [cgab],
  install: true,
)

shared_library('cgablist',
  'lib/list.c',
  link_with: [cgab],
  install: true,
)

shared_library('cgabsymbol',
  'lib/symbol.c',
  link_with: [cgab],
  install: true,
)

shared_library('cgabstring',
  'lib/string.c',
  link_with: [cgab],
  install: true,
)

shared_library('cgabrecord',
  'lib/rec.c',
  'lib/dict.h',
  'lib/list.h',
  link_with: [cgab],
  install: true,
)

shared_library('cgabany',
  'lib/any.c',
  link_with: [cgab],
  install: true,
)

shared_library('cgabnum',
  'lib/num.c',
  link_with: [cgab],
  install: true,
)

shared_library('cgabio',
  'lib/io.c',
  link_with: [cgab],
  install: true,
)

shared_library('cgabdis',
  'lib/dis.c',
  link_with: [cgab],
  install: true,
)

shared_library('cgabpry',
  'lib/pry.c',
  link_with: [cgab],
  install: true,
)

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
  )

install_data(
  'std/strings.gab',
  'std/numbers.gab',
  'std/time.gab',
  'std/lists.gab',
  'std/dictionaries.gab',
  'std/pry.gab',
  'std/test.gab',
  'std/records.gab',
  'std/http.gab',
  'std/any.gab',
)

executable('gab',
  'src/cli/main.c',
  link_with: [cgab],
  install: true,
)
