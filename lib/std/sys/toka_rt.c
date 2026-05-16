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
