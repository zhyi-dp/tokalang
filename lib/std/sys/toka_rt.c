#include <stdio.h>
#include <time.h>

void* toka_localtime_r(const time_t *timep, struct tm *result) {
#ifdef _WIN32
    struct tm *res = localtime(timep);
    if (res) { *result = *res; return result; }
    return NULL;
#else
    return localtime_r(timep, result);
#endif
}

#ifdef _WIN32
#include <io.h>
#include <fcntl.h>
int toka_setmode(int fd, int mode) { return _setmode(fd, mode); }
int toka_fileno(FILE *f) { return _fileno(f); }
#else
#include <unistd.h>
int toka_setmode(int fd, int mode) { return 0; }
int toka_fileno(FILE *f) { return fileno(f); }

#ifdef __linux__
extern int main(int argc, char **argv);
__attribute__((weak)) void _start() {
    asm volatile (
        "pop %rdi\n"       
        "mov %rsp, %rsi\n" 
        "call main\n"      
        "mov %rax, %rdi\n" 
        "mov $60, %rax\n"  
        "syscall\n"
    );
}
#endif
#endif

#ifdef _WIN32
#include <windows.h>
#include <stdint.h>
int toka_clock_realtime(int64_t *ts) {
    FILETIME ft;
    GetSystemTimeAsFileTime(&ft);
    uint64_t t = ((uint64_t)ft.dwHighDateTime << 32) | ft.dwLowDateTime;
    t -= 116444736000000000ULL;
    ts[0] = t / 10000000;
    ts[1] = (t % 10000000) * 100;
    return 1;
}
int toka_clock_monotonic(int64_t *ts) {
    LARGE_INTEGER freq, count;
    if (QueryPerformanceFrequency(&freq) && QueryPerformanceCounter(&count)) {
        ts[0] = count.QuadPart / freq.QuadPart;
        ts[1] = ((count.QuadPart % freq.QuadPart) * 1000000000) / freq.QuadPart;
        return 1;
    }
    return 0;
}
#endif

#include <stdio.h>
#include <stdlib.h>
void toka_panic(const char* msg, int len) {
    fprintf(stderr, "thread 'main' panicked at '%.*s'\n", len, msg);
    fflush(stderr);
    abort();
}

#include <sys/stat.h>
void* toka_stat_impl(const char* path) {
    struct stat* st = malloc(sizeof(struct stat));
    if (!st) return NULL;
    if (stat(path, st) != 0) {
        free(st);
        return NULL;
    }
    return st;
}
unsigned int toka_stat_mode(void* handle) { return ((struct stat*)handle)->st_mode; }
unsigned long long toka_stat_size(void* handle) { return ((struct stat*)handle)->st_size; }
long long toka_stat_mtime(void* handle) { return ((struct stat*)handle)->st_mtime; }
void toka_stat_free(void* handle) { free(handle); }
