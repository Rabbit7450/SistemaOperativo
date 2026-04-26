#ifndef PROCESS_H
#define PROCESS_H

#include <stdint.h>
#include "common.h"

extern Process process_table[MAX_PROCESSES];
extern int current_pid;
extern int next_pid;
extern int timer_ticks;

void init_process_table();
int create_process(const char *name, uint32_t mem_size);
void kill_process(int pid);
void sleep_process(int pid, int seconds);
int get_best_process();
void process_tick();

void save_context_from_regs(int pid, uint32_t *regs);
void load_context_to_regs(int pid, uint32_t *regs);

#endif
