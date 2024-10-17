#include "os.h"
#include "gab.h"

#if defined(__APPLE__)
#include <dlfcn.h>
#endif

#if defined(__linux__)
#include <dlfcn.h>
#endif

#if defined(_WIN32)
#include <strsafe.h>
#include <windows.h>
#endif

enum gab_osdl_k { kGAB_OSDL_FAIL, kGAB_OSDL_SUCCESS, kGAB_OSDL_NOPLATFORM };

struct gab_osdl_t {
#if defined(__linux__) || defined(__APPLE__)
  void *dll_handle;
#endif

#if defined(_WIN32)
  HINSTANCE dll_handle;
#endif

  enum gab_osdl_k k;
};

#if defined(_WIN32)
void platform_win32_get_last_error(LPTSTR lpszFunction) {
  LPVOID lpMsgBuf;
  LPVOID lpDisplayBuf;
  DWORD dw = GetLastError();

  FormatMessage(FORMAT_MESSAGE_ALLOCATE_BUFFER | FORMAT_MESSAGE_FROM_SYSTEM |
                    FORMAT_MESSAGE_IGNORE_INSERTS,
                NULL, dw, MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT),
                (LPTSTR)&lpMsgBuf, 0, NULL);

  // Display the error message and exit the process

  lpDisplayBuf =
      (LPVOID)LocalAlloc(LMEM_ZEROINIT, (lstrlen((LPCTSTR)lpMsgBuf) +
                                         lstrlen((LPCTSTR)lpszFunction) + 40) *
                                            sizeof(TCHAR));
  StringCchPrintf((LPTSTR)lpDisplayBuf, LocalSize(lpDisplayBuf) / sizeof(TCHAR),
                  TEXT("%s failed with error %d: %s"), lpszFunction, dw,
                  lpMsgBuf);

  printf("Error message: %s", (LPCTSTR)lpDisplayBuf);

  LocalFree(lpMsgBuf);
  LocalFree(lpDisplayBuf);
}

#endif

struct gab_osdl_t gab_osdlopen(const char *file) {

  struct gab_osdl_t dll = {0};

// UNIX specific code(MacOS uses the same API as linux for dlopen)
#if defined(__linux__) || defined(__APPLE__)
  dll.dll_handle = dlopen(file, RTLD_NOW);

  if (dll.dll_handle != nullptr)
    dll.k = kGAB_OSDL_SUCCESS;
#endif

// Windows specific code
#if defined(_WIN32)
  dll.dll_handle = LoadLibrary(file);

  if (dll.dll_handle)
    dll.k = kGAB_OSDL_SUCCESS;
#endif

  return dll;
}

void *gab_osdlsym(struct gab_osdl_t dll, const char *symbol) {
#if defined(_WIN32)
  return GetProcAddress(dll.dll_handle, symbol);
#endif

#if defined(__linux) || defined(__APPLE__)
  return dlsym(dll.dll_handle, symbol);
#endif
}

a_gab_value *gab_osloaddynmod(struct gab_triple gab, const char *path) {
  struct gab_osdl_t dl = gab_osdlopen(path);

  if (dl.k == kGAB_OSDL_FAIL)
    return gab_fpanic(gab, "Failed to load module $: $", gab_string(gab, path), gab_string(gab, dlerror()));

  gab_osdynmod symbol = gab_osdlsym(dl, GAB_DYNAMIC_MODULE_SYMBOL);

  if (!symbol)
    return gab_fpanic(gab, "Failed to load symbol $",
                      gab_string(gab, GAB_DYNAMIC_MODULE_SYMBOL));

  return symbol(gab);
}

enum gab_osdl_k gab_osdlclose(struct gab_osdl_t handle) {
#if defined(__linux) || defined(__APPLE__)
  if (dlclose(handle.dll_handle) != 0)
    return kGAB_OSDL_FAIL;
  else
    return kGAB_OSDL_SUCCESS;
#endif

#if defined(__WIN32)
  if (FreeLibrary(handle.dll_handle))
    return kGAB_OSDL_SUCCESS;
  else
    return kGAB_OSDL_FAILED;
#endif

  return kGAB_OSDL_NOPLATFORM;
}

a_char *gab_fosread(FILE *fd) {
  v_char buffer = {0};

  while (1) {
    int c = fgetc(fd);

    if (c == EOF)
      break;

    v_char_push(&buffer, c);
  }

  v_char_push(&buffer, '\0');

  a_char *data = a_char_create(buffer.data, buffer.len);

  v_char_destroy(&buffer);

  return data;
}

a_char *gab_osread(const char *path) {
  FILE *file = fopen(path, "rb");

  if (file == nullptr)
    return nullptr;

  a_char *data = gab_fosread(file);

  fclose(file);
  return data;
}

a_char *gab_fosreadl(FILE *fd) {
  v_char buffer;
  v_char_create(&buffer, 1024);

  for (;;) {
    int c = fgetc(fd);

    v_char_push(&buffer, c);

    if (c == '\n' || c == EOF)
      break;
  }

  v_char_push(&buffer, '\0');

  a_char *data = a_char_create(buffer.data, buffer.len);

  v_char_destroy(&buffer);

  return data;
}
