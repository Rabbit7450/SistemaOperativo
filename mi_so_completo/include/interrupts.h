#ifndef INTERRUPTS_H
#define INTERRUPTS_H

#include <stdint.h>

void setup_idt();
char get_key();
void run_user_process(int pid);

#endif
