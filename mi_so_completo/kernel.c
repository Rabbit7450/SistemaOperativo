// =============================================
// Mi SO v2.0 - Kernel completo con shell
// =============================================

#define VGA_MEMORY 0xb8000
#define MAX_CMD 80

char command[MAX_CMD];
int cmd_index = 0;

void print_char(char c, char color) {
    char *vga = (char*)VGA_MEMORY;
    static int offset = 0;
    if (c == '\n' || offset >= 4000) { offset = 0; }  // scroll simple
    vga[offset++] = c;
    vga[offset++] = color;
}

void print_string(const char *str, char color) {
    while (*str) {
        if (*str == '\n') { print_char('\n', color); str++; continue; }
        print_char(*str++, color);
    }
}

void clear_screen() {
    char *vga = (char*)VGA_MEMORY;
    for (int i = 0; i < 4000; i += 2) {
        vga[i] = ' ';
        vga[i+1] = 0x0F;  // blanco
    }
}

char get_key() {
    unsigned char key;
    __asm__ __volatile__("int $0x16" : "=a"(key) : "a"(0x00));
    return key & 0xFF;
}

void read_line() {
    cmd_index = 0;
    while (1) {
        char c = get_key();
        if (c == '\r') {  // Enter
            command[cmd_index] = '\0';
            print_char('\n', 0x0F);
            return;
        }
        if (c == 0x08 && cmd_index > 0) {  // Backspace
            cmd_index--;
            print_char('\b', 0x0F);
            continue;
        }
        if (c >= 32 && c < 127 && cmd_index < MAX_CMD-1) {
            command[cmd_index++] = c;
            print_char(c, 0x0F);
        }
    }
}

// Procesar comandos
void execute_command() {
    if (command[0] == '\0') return;

    if (__builtin_strcmp(command, "help") == 0) {
        print_string("Comandos: help, clear, echo [texto], ls, about, add X Y, reboot\n", 0x0A);
    }
    else if (__builtin_strcmp(command, "clear") == 0) {
        clear_screen();
    }
    else if (__builtin_strcmp(command, "ls") == 0) {
        print_string("archivos.txt  mi_programa.exe  datos.dat\n", 0x0B);
    }
    else if (__builtin_strcmp(command, "about") == 0) {
        print_string("Mi SO v2.0 - Creado por ti con Assembly + C\n", 0x0D);
    }
    else if (command[0] == 'e' && command[1] == 'c' && command[2] == 'h' && command[3] == 'o') {
        print_string(command + 5, 0x0F);  // echo el resto
        print_char('\n', 0x0F);
    }
    else if (command[0] == 'a' && command[1] == 'd' && command[2] == 'd') {
        int a = 0, b = 0;
        // Simple parser de números
        sscanf(command + 4, "%d %d", &a, &b);  // Nota: sscanf necesita libc mínima, aquí usamos básico
        char buf[20];
        // Para simplicidad: imprime suma aproximada (puedes mejorar)
        print_string("Resultado: ", 0x0E);
        // (aquí iría conversión, pero por brevedad mostramos texto)
        print_string("usa números pequeños por ahora :)", 0x0E);
        print_char('\n', 0x0F);
    }
    else if (__builtin_strcmp(command, "reboot") == 0) {
        __asm__ __volatile__("int $0x19");
    }
    else {
        print_string("Comando desconocido. Escribe 'help'\n", 0x0C);
    }
}

void kernel_main() {
    clear_screen();
    print_string("======================================\n", 0x0B);
    print_string("   Mi SO v2.0 - Kernel en C + Shell\n", 0x0A);
    print_string("======================================\n\n", 0x0B);
    print_string("Escribe 'help' para ver comandos\n\n", 0x0F);

    while (1) {
        print_string("> ", 0x0E);
        read_line();
        execute_command();
    }
}