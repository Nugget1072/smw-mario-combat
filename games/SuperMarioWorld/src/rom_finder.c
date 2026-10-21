// Lists *.sfc / *.smc files in the current directory (used to find the player's own ROM next to smw.exe).
#include <stdio.h>
#include <string.h>
#ifdef _WIN32
#include <windows.h>
#else
#include <dirent.h>
#endif

int FindRomFiles(char (*out)[260], int max) {
  int n = 0;
#ifdef _WIN32
  const char *patterns[2] = { "*.sfc", "*.smc" };
  for (int p = 0; p < 2; p++) {
    WIN32_FIND_DATAA fd;
    HANDLE fh = FindFirstFileA(patterns[p], &fd);
    if (fh == INVALID_HANDLE_VALUE)
      continue;
    do {
      if (n < max)
        snprintf(out[n++], 260, "%s", fd.cFileName);
    } while (FindNextFileA(fh, &fd));
    FindClose(fh);
  }
#else
  DIR *d = opendir(".");
  struct dirent *e;
  while (d && (e = readdir(d)) != NULL && n < max) {
    size_t l = strlen(e->d_name);
    if (l > 4 && (!strcmp(e->d_name + l - 4, ".sfc") || !strcmp(e->d_name + l - 4, ".smc")))
      snprintf(out[n++], 260, "%s", e->d_name);
  }
  if (d) closedir(d);
#endif
  return n;
}
