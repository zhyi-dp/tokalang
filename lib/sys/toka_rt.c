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
const char* toka_readdir_name(void* entry) { return NULL; }
void* toka_opendir_impl(const char* path) { return NULL; }
void* toka_readdir_impl(void* dir) { return NULL; }
void toka_closedir_impl(void* dir) {}
void* toka_stat_impl(const char* path) { return NULL; }
unsigned int toka_stat_mode(void* handle) { return 0; }
unsigned long long toka_stat_size(void* handle) { return 0; }
long long toka_stat_mtime(void* handle) { return 0; }
void toka_stat_free(void* handle) {}
int toka_fileno(FILE *f) { return _fileno(f); }

#else
#include <unistd.h>
int toka_setmode(int fd, int mode) { return 0; }
int toka_fileno(FILE *f) { return fileno(f); }

#if 0
#ifdef __linux__
extern int main(int argc, char **argv);
__attribute__((naked, weak)) void _start() {
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
#endif

#ifdef _WIN32
#include <windows.h>
int toka_get_last_error() { return GetLastError(); }
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
    const char *prefix = "thread 'main' panicked at '";
    const char *suffix = "'\n";
#ifdef _WIN32
    _write(2, prefix, 27);
    _write(2, msg, len);
    _write(2, suffix, 2);
#else
    write(2, prefix, 27);
    write(2, msg, len);
    write(2, suffix, 2);
#endif
    abort();
}

#include <sys/stat.h>
#ifndef _WIN32
#include <dirent.h>
#include <errno.h>
int toka_get_last_error() { return errno; }
const char* toka_readdir_name(void* entry) { return ((struct dirent*)entry)->d_name; }
void* toka_opendir_impl(const char* path) { return opendir(path); }
void* toka_readdir_impl(void* dir) { return readdir(dir); }
void toka_closedir_impl(void* dir) { closedir(dir); }
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
#endif

#ifdef _WIN32
#include <winsock2.h>
#include <ws2tcpip.h>
#else
#include <sys/types.h>
#include <sys/socket.h>
#include <netdb.h>
#include <arpa/inet.h>
#endif

unsigned int toka_resolve_ipv4(const char* host) {
    struct addrinfo hints = {0};
    hints.ai_family = AF_INET;
    hints.ai_socktype = SOCK_STREAM;
    struct addrinfo *res;
    if (getaddrinfo(host, NULL, &hints, &res) != 0) return 0;
    struct sockaddr_in *addr = (struct sockaddr_in *)res->ai_addr;
    unsigned int ip = addr->sin_addr.s_addr;
    freeaddrinfo(res);
    return ip;
}

void toka_print_str(const char* s) {
    printf("%s", s);
}

void toka_print_i32(int val) {
    printf("%d", val);
}

void toka_print_f64(double val) {
    printf("%g", val);
}

