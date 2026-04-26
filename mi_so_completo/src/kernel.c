#include "vga.h"
#include "process.h"
#include "servers.h"
#include "interrupts.h"
#include "shell.h"

void kernel_main() {
    clear_screen();
    print_string("======================================\n", 0x0B);
    print_string("   Mi SO v3.3 - Arquitectura Modular\n", 0x0A);
    print_string("   Kernel + Shell + IRQ + Procesos\n", 0x0A);
    print_string("======================================\n", 0x0B);
    print_string("   Modo: Protegido 32-bit + Ring 3\n", 0x0E);
    print_string("   Plataforma: i386\n", 0x0E);
    print_string("======================================\n\n", 0x0B);

    init_process_table();
    init_server_manager();
    init_server_filesystem();
    setup_idt();

    print_string("Subsistemas cargados:\n", 0x0E);
    print_string("  > VGA / Consola\n", 0x0E);
    print_string("  > Scheduler + Context Switch\n", 0x0E);
    print_string("  > IDT + PIC + IRQ0/IRQ1 + INT 0x80\n", 0x0E);
    print_string("  > Shell modular\n", 0x0E);
    print_string("  > Server manager simulado\n\n", 0x0E);

    print_string("Escribe 'help' para ver comandos\n\n", 0x0F);

    while (1) {
        print_string("> ", 0x0E);
        shell_read_line();
        execute_command();
    }
}
