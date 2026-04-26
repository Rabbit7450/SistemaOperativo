#include "common.h"
#include "process.h"
#include "servers.h"
#include "vga.h"
#include "utils.h"

Process process_table[MAX_PROCESSES];
int current_pid = 0;
int next_pid = 1;
int timer_ticks = 0;

#define REG_EDI 0
#define REG_ESI 1
#define REG_EBP 2
#define REG_ESP 3
#define REG_EBX 4
#define REG_EDX 5
#define REG_ECX 6
#define REG_EAX 7

void user_program();

void init_process_table() {
    for (int i = 0; i < MAX_PROCESSES; i++) {
        process_table[i].pid = -1;
        process_table[i].state = PROC_EMPTY;
    }

    process_table[0].pid = 0;
    process_table[0].state = PROC_RUNNING;
    process_table[0].priority = 1;
    process_table[0].memory_start = 0x1000;
    process_table[0].memory_size = 0x7000;
    process_table[0].mode = KERNEL_MODE;
    process_table[0].sleep_ticks = 0;
    process_table[0].cpu_time = 0;
    process_table[0].context.valid = 1;
    process_table[0].context.cs = 0x08;
    process_table[0].context.ss = 0x10;
    process_table[0].context.eflags = 0x202;
    copy_string(process_table[0].name, "kernel", 32);
}

int create_process(const char *name, uint32_t mem_size) {
    if (next_pid >= MAX_PROCESSES) {
        print_string("Error: tabla de procesos llena\n", 0x0C);
        return -1;
    }

    int pid = next_pid;
    process_table[pid].pid = pid;
    process_table[pid].state = PROC_READY;
    process_table[pid].priority = 5;
    process_table[pid].memory_size = mem_size;
    process_table[pid].mode = USER_MODE;
    process_table[pid].sleep_ticks = 0;
    process_table[pid].cpu_time = 0;

    uint32_t base = 0x10000;
    for (int i = 1; i < pid; i++) {
        base += process_table[i].memory_size;
    }
    process_table[pid].memory_start = base;

    process_table[pid].context.eax = 0;
    process_table[pid].context.ebx = 0;
    process_table[pid].context.ecx = 0;
    process_table[pid].context.edx = 0;
    process_table[pid].context.esi = 0;
    process_table[pid].context.edi = 0;
    process_table[pid].context.ebp = 0;
    process_table[pid].context.eip = (uint32_t)user_program;
    process_table[pid].context.esp = base + mem_size - 4;
    process_table[pid].context.cs = 0x1B;
    process_table[pid].context.ss = 0x23;
    process_table[pid].context.eflags = 0x202;
    process_table[pid].context.valid = 1;

    copy_string(process_table[pid].name, name, 32);
    next_pid++;

    return pid;
}

void kill_process(int pid) {
    if (pid < 0 || pid >= MAX_PROCESSES || process_table[pid].state == PROC_EMPTY) {
        print_string("Error: proceso no existe\n", 0x0C);
        return;
    }
    if (pid == 0) {
        print_string("Error: no se puede matar el kernel\n", 0x0C);
        return;
    }
    process_table[pid].state = PROC_ZOMBIE;
}

void sleep_process(int pid, int seconds) {
    if (pid < 0 || pid >= MAX_PROCESSES || process_table[pid].state == PROC_EMPTY) {
        print_string("Error: proceso no existe\n", 0x0C);
        return;
    }
    if (pid == 0) {
        print_string("Error: no se puede suspender el kernel\n", 0x0C);
        return;
    }
    if (seconds <= 0) {
        print_string("Error: segundos debe ser mayor a 0\n", 0x0C);
        return;
    }

    process_table[pid].state = PROC_SLEEP;
    process_table[pid].sleep_ticks = seconds * 100;
}

int get_best_process() {
    int best_pid = -1;
    int best_priority = 10;

    for (int i = 0; i < MAX_PROCESSES; i++) {
        if (process_table[i].state == PROC_READY) {
            if (process_table[i].priority < best_priority) {
                best_priority = process_table[i].priority;
                best_pid = i;
            }
        }
    }

    if (best_pid == -1) {
        return current_pid;
    }
    return best_pid;
}

void save_context_from_regs(int pid, uint32_t *regs) {
    if (pid < 0 || pid >= MAX_PROCESSES || process_table[pid].state == PROC_EMPTY) {
        return;
    }

    process_table[pid].context.edi = regs[REG_EDI];
    process_table[pid].context.esi = regs[REG_ESI];
    process_table[pid].context.ebp = regs[REG_EBP];
    process_table[pid].context.esp = regs[REG_ESP];
    process_table[pid].context.ebx = regs[REG_EBX];
    process_table[pid].context.edx = regs[REG_EDX];
    process_table[pid].context.ecx = regs[REG_ECX];
    process_table[pid].context.eax = regs[REG_EAX];
    process_table[pid].context.valid = 1;
}

void load_context_to_regs(int pid, uint32_t *regs) {
    if (pid < 0 || pid >= MAX_PROCESSES || !process_table[pid].context.valid) {
        return;
    }

    regs[REG_EDI] = process_table[pid].context.edi;
    regs[REG_ESI] = process_table[pid].context.esi;
    regs[REG_EBP] = process_table[pid].context.ebp;
    regs[REG_ESP] = process_table[pid].context.esp;
    regs[REG_EBX] = process_table[pid].context.ebx;
    regs[REG_EDX] = process_table[pid].context.edx;
    regs[REG_ECX] = process_table[pid].context.ecx;
    regs[REG_EAX] = process_table[pid].context.eax;
}

void process_tick() {
    timer_ticks++;
    server_tick(timer_ticks);

    for (int i = 0; i < MAX_PROCESSES; i++) {
        if (process_table[i].state == PROC_SLEEP) {
            process_table[i].sleep_ticks--;
            if (process_table[i].sleep_ticks <= 0) {
                process_table[i].state = PROC_READY;
            }
        }
    }

    for (int i = 1; i < MAX_PROCESSES; i++) {
        if (process_table[i].state == PROC_ZOMBIE) {
            process_table[i].state = PROC_EMPTY;
            process_table[i].pid = -1;
        }
    }

    if (timer_ticks % 10 == 0) {
        if (process_table[current_pid].state == PROC_RUNNING) {
            process_table[current_pid].state = PROC_READY;
        }

        int next_process = get_best_process();
        if (next_process != -1 && process_table[next_process].state == PROC_READY) {
            current_pid = next_process;
            process_table[current_pid].state = PROC_RUNNING;
            process_table[current_pid].cpu_time++;
        }
    }
}
