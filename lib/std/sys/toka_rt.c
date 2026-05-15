#include <stdio.h>

#ifdef _WIN32
#include <io.h>
#include <fcntl.h>
int toka_setmode(int fd, int mode) { return _setmode(fd, mode); }
int toka_fileno(FILE *f) { return _fileno(f); }
#else
#include <unistd.h>
int toka_setmode(int fd, int mode) { return 0; }
int toka_fileno(FILE *f) { return fileno(f); }
#endif
