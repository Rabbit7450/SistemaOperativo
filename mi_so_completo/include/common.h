#ifndef COMMON_H
#define COMMON_H

#include <stdint.h>

#define VGA_MEMORY 0xb8000
#define MAX_CMD 80
#define MAX_PROCESSES 10
#define MAX_SERVERS 8
#define MAX_SERVER_DIRS 6
#define MAX_SERVER_FILES 12
#define MAX_FILE_CONTENT 64
#define MAX_AUDIT_LOGS 32

#define KERNEL_MODE 0
#define USER_MODE 1

#define PROC_EMPTY 0
#define PROC_RUNNING 1
#define PROC_READY 2
#define PROC_BLOCKED 3
#define PROC_SLEEP 4
#define PROC_ZOMBIE 5

#define SYS_EXIT 1
#define SYS_GETPID 2
#define SYS_GETMEM 3
#define SYS_SLEEP 4
#define SYS_SETPRIO 5
#define SYS_GETPRIO 6
#define SYS_WRITE 7

typedef struct {
    uint32_t eax, ebx, ecx, edx;
    uint32_t esi, edi, ebp;
    uint32_t eip, esp;
    uint32_t cs, ss, eflags;
    int valid;
} UserContext;

typedef struct {
    int pid;
    char name[32];
    int state;
    int priority;
    uint32_t memory_start;
    uint32_t memory_size;
    int mode;
    UserContext context;
    int sleep_ticks;
    int cpu_time;
    int wait_ticks;
} Process;

typedef struct {
    char name[16];
    int active;
    int cpu_load;
    int memory_mb;
    int health;
    int restarts;
} ServerService;

typedef struct {
    int used;
    char name[16];
} ServerDirectory;

typedef struct {
    int used;
    char dir[16];
    char name[16];
    char content[MAX_FILE_CONTENT];
    int perm_read;
    int perm_write;
} ServerFile;

typedef struct {
    ServerDirectory dirs[MAX_SERVER_DIRS];
    ServerFile files[MAX_SERVER_FILES];
    int valid;
} ServerSnapshot;

#endif
