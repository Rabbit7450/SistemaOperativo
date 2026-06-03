#include "common.h"
#include "shell.h"
#include "vga.h"
#include "utils.h"
#include "process.h"
#include "servers.h"
#include "interrupts.h"

static char command[MAX_CMD];
static int cmd_index = 0;

void shell_read_line() {
    cmd_index = 0;
    while (1) {
        char c = get_key();
        if (c == '\r') {
            command[cmd_index] = '\0';
            print_char('\n', 0x0F);
            return;
        }
        if (c == 0x08 && cmd_index > 0) {
            cmd_index--;
            command[cmd_index] = '\0';
            print_char('\b', 0x0F);
            continue;
        }
        if (c >= 32 && c < 127 && cmd_index < MAX_CMD - 1) {
            command[cmd_index++] = c;
            command[cmd_index] = '\0';
            print_char(c, 0x0F);
        }
    }
}

void execute_command() {
    if (command[0] == '\0') return;

    if (__builtin_strcmp(command, "help") == 0) {
        print_string("\n", 0x0F);
        print_string("═══════════════════════════════════════════════════════════\n", 0x0E);
        print_string("       MANUAL DEL SISTEMA OPERATIVO v3.3 MODULAR\n", 0x0E);
        print_string("═══════════════════════════════════════════════════════════\n", 0x0E);
        
        print_string("\n[SYSCALLS NUEVAS]\n", 0x0A);
        print_string("  getppid PID  - Ver PID del proceso padre\n", 0x0F);
        print_string("  fork         - Crear nuevo proceso hijo\n", 0x0F);
        print_string("  wait PID     - Esperar a que proceso termine\n", 0x0F);
        print_string("  yield        - Ceder CPU voluntariamente\n", 0x0F);
        print_string("  read         - Leer entrada del teclado\n", 0x0F);
        print_string("  stats        - Ver estadísticas del SO\n", 0x0F);
        
        print_string("\n[MONITOREO]\n", 0x0A);
        print_string("  ps           - Listar procesos activos\n", 0x0F);
        print_string("  top          - Monitor detallado de procesos\n", 0x0F);
        print_string("  memory       - Ver uso de memoria\n", 0x0F);
        print_string("  uptime       - Ver tiempo de actividad\n", 0x0F);
        
        print_string("\n[PROCESOS]\n", 0x0A);
        print_string("  exec NAME    - Crear proceso usuario\n", 0x0F);
        print_string("  runu PID     - Ejecutar proceso en Ring 3\n", 0x0F);
        print_string("  kill PID     - Terminar proceso\n", 0x0F);
        print_string("  sleep PID S  - Dormir proceso S segundos\n", 0x0F);
        print_string("  priority P P - Cambiar prioridad (0-9)\n", 0x0F);
        
        print_string("\n[SISTEMA]\n", 0x0A);
        print_string("  clear        - Limpiar pantalla\n", 0x0F);
        print_string("  about        - Información del SO\n", 0x0F);
        print_string("  help         - Este mensaje\n", 0x0F);
        print_string("  help syscalls- Listar syscalls disponibles\n", 0x0F);
        
        print_string("\n[HERRAMIENTAS]\n", 0x0A);
        print_string("  add X Y      - Sumar dos números\n", 0x0F);
        print_string("  echo TEXTO   - Imprimir texto\n", 0x0F);
        
        print_string("\n═══════════════════════════════════════════════════════════\n", 0x0E);
        print_string("\n", 0x0F);
    }
    else if (__builtin_strcmp(command, "help syscalls") == 0) {
        print_string("Syscalls disponibles:\n", 0x0A);
        print_string("  1  SYS_EXIT(code)        - Terminar proceso\n", 0x0A);
        print_string("  2  SYS_GETPID()          - Obtener PID actual\n", 0x0A);
        print_string("  3  SYS_GETMEM()          - Obtener tamaño memoria\n", 0x0A);
        print_string("  4  SYS_SLEEP(seconds)    - Dormir proceso\n", 0x0A);
        print_string("  5  SYS_SETPRIO(pid, p)   - Cambiar prioridad\n", 0x0A);
        print_string("  6  SYS_GETPRIO(pid)      - Obtener prioridad\n", 0x0A);
        print_string("  7  SYS_WRITE(buf, len)   - Escribir a pantalla\n", 0x0A);
        print_string("  8  SYS_READ(buf, len)    - Leer teclado\n", 0x0A);
        print_string("  9  SYS_FORK()            - Crear proceso hijo\n", 0x0A);
        print_string("  10 SYS_WAIT(pid)         - Esperar proceso\n", 0x0A);
        print_string("  11 SYS_YIELD()           - Ceder CPU\n", 0x0A);
        print_string("  12 SYS_GETPPID()         - Obtener PID padre\n", 0x0A);
    }
    else if (__builtin_strcmp(command, "cmd") == 0) {
        print_string("CMD Integrado - Gestion Linux\n", 0x0B);
        print_string("Rol: ", 0x0B);
        print_string(role_name(current_role), 0x0B);
        print_string(" | Sesion: ", 0x0B);
        print_string(session_authenticated ? "activa\n" : "cerrada\n", 0x0B);
    }
    else if (__builtin_strcmp(command, "principales") == 0) {
        command_principales();
    }
    else if (__builtin_strcmp(command, "whoami") == 0) {
        print_string("Rol: ", 0x0E);
        print_string(role_name(current_role), 0x0E);
        print_string(" | Sesion: ", 0x0E);
        print_string(session_authenticated ? "activa\n" : "cerrada\n", 0x0E);
    }
    else if (starts_with(command, "login ")) {
        const char *cursor = command + 6;
        char role[16];
        char pass[24];
        if (!read_token(&cursor, role, 16) || !read_token(&cursor, pass, 24)) {
            print_string("Uso: login <admin|operator|viewer> <clave>\n", 0x0C);
        } else {
            command_login(role, pass);
        }
    }
    else if (__builtin_strcmp(command, "logout") == 0) {
        command_logout();
    }
    else if (__builtin_strcmp(command, "clear") == 0) {
        clear_screen();
    }
    else if (__builtin_strcmp(command, "ps") == 0) {
        print_string("\n", 0x0F);
        print_string("  PID NOMBRE           ESTADO  PRIO   PADRE  MEM\n", 0x0E);
        print_string("  --- -----------      ------- ----   -----  ----\n", 0x0E);
        for (int i = 0; i < MAX_PROCESSES; i++) {
            if (process_table[i].state != PROC_EMPTY) {
                char buf[20];
                print_string("  ", 0x0F);
                int_to_str(process_table[i].pid, buf);
                if (process_table[i].pid < 10) print_string(" ", 0x0F);
                print_string(buf, 0x0F);
                print_string(" ", 0x0F);
                print_string(process_table[i].name, 0x0F);
                for (int j = __builtin_strlen(process_table[i].name); j < 16; j++) print_string(" ", 0x0F);
                print_string(" ", 0x0F);
                switch (process_table[i].state) {
                    case PROC_RUNNING: print_string("RUN    ", 0x0A); break;
                    case PROC_READY: print_string("READY  ", 0x0B); break;
                    case PROC_SLEEP: print_string("SLEEP  ", 0x0E); break;
                    case PROC_ZOMBIE: print_string("ZOMBIE ", 0x0C); break;
                    default: print_string("?      ", 0x0F); break;
                }
                int_to_str(process_table[i].priority, buf);
                if (process_table[i].priority < 10) print_string(" ", 0x0F);
                print_string(buf, 0x0F);
                print_string("    ", 0x0F);
                int_to_str(process_table[i].parent_pid, buf);
                if (process_table[i].parent_pid < 10) print_string(" ", 0x0F);
                print_string(buf, 0x0F);
                print_string("     ", 0x0F);
                int_to_str(process_table[i].memory_size / 1024, buf);
                print_string(buf, 0x0F);
                print_string("K\n", 0x0F);
            }
        }
        print_string("\n", 0x0F);
    }
    else if (__builtin_strcmp(command, "top") == 0) {
        print_string("\n", 0x0F);
        print_string("===== MONITOR DE PROCESOS (TOP) ====\n", 0x0E);
        print_string("PID  NAME          STATE   PRIO MEM(K) CPU  PPID\n", 0x0E);
        print_string("--- -------------- ------- ---- ------ ---- ----\n", 0x0E);
        for (int i = 0; i < MAX_PROCESSES; i++) {
            if (process_table[i].state != PROC_EMPTY) {
                char buf[20];
                int_to_str(process_table[i].pid, buf);
                if (process_table[i].pid < 10) print_string(" ", 0x0F);
                print_string(buf, 0x0F);
                print_string("  ", 0x0F);
                print_string(process_table[i].name, 0x0F);
                for (int j = __builtin_strlen(process_table[i].name); j < 14; j++) print_string(" ", 0x0F);
                switch (process_table[i].state) {
                    case PROC_RUNNING: print_string("RUN    ", 0x0A); break;
                    case PROC_READY: print_string("READY  ", 0x0B); break;
                    case PROC_SLEEP: print_string("SLEEP  ", 0x0E); break;
                    case PROC_ZOMBIE: print_string("ZOMBIE ", 0x0C); break;
                    default: print_string("?      ", 0x0F); break;
                }
                int_to_str(process_table[i].priority, buf);
                if (process_table[i].priority < 10) print_string(" ", 0x0F);
                print_string(buf, 0x0F);
                print_string("  ", 0x0F);
                int_to_str(process_table[i].memory_size / 1024, buf);
                if (process_table[i].memory_size / 1024 < 100) print_string(" ", 0x0F);
                if (process_table[i].memory_size / 1024 < 10) print_string(" ", 0x0F);
                print_string(buf, 0x0F);
                print_string("  ", 0x0F);
                int_to_str(process_table[i].cpu_time, buf);
                if (process_table[i].cpu_time < 10) print_string(" ", 0x0F);
                print_string(buf, 0x0F);
                print_string("  ", 0x0F);
                int_to_str(process_table[i].parent_pid, buf);
                if (process_table[i].parent_pid < 10) print_string(" ", 0x0F);
                print_string(buf, 0x0F);
                print_string("\n", 0x0F);
            }
        }
        print_string("=====================================\n", 0x0E);
        print_string("\n", 0x0F);
    }
    else if (starts_with(command, "exec ")) {
        int pid = create_process(command + 5, 4096);
        if (pid >= 0) {
            char buf[20];
            print_string("Proceso creado PID ", 0x0A);
            int_to_str(pid, buf);
            print_string(buf, 0x0A);
            print_string("\n", 0x0A);
        }
    }
    else if (starts_with(command, "runu ")) {
        int pid = parse_number(command + 5);
        run_user_process(pid);
    }
    else if (starts_with(command, "kill ")) {
        kill_process(parse_number(command + 5));
    }
    else if (starts_with(command, "sleep ")) {
        int i = 6;
        int pid = parse_number(command + i);
        while (command[i] != ' ' && command[i] != '\0') i++;
        while (command[i] == ' ') i++;
        int seconds = parse_number(command + i);
        sleep_process(pid, seconds);
    }
    else if (starts_with(command, "priority ")) {
        int i = 9;
        int pid = parse_number(command + i);
        while (command[i] != ' ' && command[i] != '\0') i++;
        while (command[i] == ' ') i++;
        int prio = parse_number(command + i);
        if (pid >= 0 && pid < MAX_PROCESSES && process_table[pid].state != PROC_EMPTY && prio >= 0 && prio <= 9) {
            process_table[pid].priority = prio;
            print_string("Prioridad actualizada\n", 0x0A);
        } else {
            print_string("Error: argumentos invalidos\n", 0x0C);
        }
    }
    else if (__builtin_strcmp(command, "memory") == 0) {
        uint32_t used = 0;
        for (int i = 0; i < MAX_PROCESSES; i++) {
            if (process_table[i].state != PROC_EMPTY) {
                used += process_table[i].memory_size;
            }
        }
        char buf[20];
        print_string("Usado: ", 0x0B);
        int_to_str(used / 1024, buf);
        print_string(buf, 0x0B);
        print_string(" KB\n", 0x0B);
    }
    else if (__builtin_strcmp(command, "uptime") == 0) {
        char buf[20];
        int_to_str(timer_ticks / 100, buf);
        print_string("Uptime: ", 0x0E);
        print_string(buf, 0x0E);
        print_string(" s\n", 0x0E);
    }
    else if (starts_with(command, "add ")) {
        int a = parse_number(command + 4);
        int i = 4;
        while (command[i] != '\0' && command[i] != ' ') i++;
        while (command[i] == ' ') i++;
        int b = parse_number(command + i);
        char buf[20];
        int_to_str(a + b, buf);
        print_string(buf, 0x0E);
        print_string("\n", 0x0E);
    }
    else if (starts_with(command, "echo ")) {
        print_string(skip_spaces(command + 5), 0x0F);
        print_string("\n", 0x0F);
    }
    else if (__builtin_strcmp(command, "about") == 0) {
        print_string("Mi SO v3.3 modular\n", 0x0D);
        print_string("Con syscalls avanzadas: fork, wait, yield, read, getppid\n", 0x0D);
    }
    else if (__builtin_strcmp(command, "server list") == 0) {
        command_server_list();
    }
    else if (starts_with(command, "server status ")) {
        command_server_status(skip_spaces(command + 14));
    }
    else if (starts_with(command, "server start ")) {
        command_server_start(skip_spaces(command + 13));
    }
    else if (starts_with(command, "server stop ")) {
        command_server_stop(skip_spaces(command + 12));
    }
    else if (starts_with(command, "server restart ")) {
        command_server_restart(skip_spaces(command + 15));
    }
    else if (starts_with(command, "server backup ")) {
        command_server_backup(skip_spaces(command + 14));
    }
    else if (starts_with(command, "server restore ")) {
        command_server_restore(skip_spaces(command + 15));
    }
    else if (__builtin_strcmp(command, "server audit") == 0) {
        command_server_audit();
    }
    else if (starts_with(command, "apache ")) {
        const char *cursor = command + 7;
        char op[16];
        if (!read_token(&cursor, op, 16)) {
            print_string("Uso: apache <start|stop|status|logs|simulate N>\n", 0x0C);
        } else if (__builtin_strcmp(op, "start") == 0) {
            command_server_start("apache");
        } else if (__builtin_strcmp(op, "stop") == 0) {
            command_server_stop("apache");
        } else if (__builtin_strcmp(op, "status") == 0) {
            command_server_status("apache");
        } else if (__builtin_strcmp(op, "logs") == 0) {
            command_apache_logs();
        } else if (__builtin_strcmp(op, "ls") == 0) {
            command_apache_ls();
        } else if (__builtin_strcmp(op, "cat") == 0) {
            while (*cursor == ' ') cursor++;
            if (*cursor == '\0') print_string("Uso: apache cat <path>\n", 0x0C);
            else command_apache_cat(skip_spaces(cursor));
        } else if (__builtin_strcmp(op, "tail") == 0) {
            while (*cursor == ' ') cursor++;
            int n = 5;
            if (*cursor != '\0') n = parse_number(cursor);
            command_apache_tail(n);
        } else if (__builtin_strcmp(op, "add") == 0) {
            char name[32];
            if (!read_token(&cursor, name, 32)) { print_string("Uso: apache add <name> <content>\n", 0x0C); }
            else {
                while (*cursor == ' ') cursor++;
                const char *content = skip_spaces(cursor);
                if (*content == '\0') print_string("Uso: apache add <name> <content>\n", 0x0C);
                else command_apache_add(name, content);
            }
        } else if (__builtin_strcmp(op, "rm") == 0) {
            char name[32];
            if (!read_token(&cursor, name, 32)) { print_string("Uso: apache rm <name>\n", 0x0C); }
            else command_apache_rm(name);
        } else if (__builtin_strcmp(op, "vhosts") == 0) {
            command_apache_vhosts();
        } else if (__builtin_strcmp(op, "simulate") == 0) {
            int n = 1;
            // try parse number
            while (*cursor == ' ') cursor++;
            if (*cursor != '\0') n = parse_number(cursor);
            if (n <= 0) n = 1;
            command_apache_simulate(n);
        } else {
            print_string("Operacion apache no valida\n", 0x0C);
        }
    }
    else if (starts_with(command, "server fs ")) {
        const char *cursor = command + 10;
        char op[16], srv[16], dir[16], file[16], mode[8];
        if (!read_token(&cursor, op, 16)) {
            print_string("Uso: server fs <op>\n", 0x0C);
        }
        else if (__builtin_strcmp(op, "dirs") == 0) {
            if (read_token(&cursor, srv, 16)) command_server_fs_dirs(srv);
            else print_string("Uso: server fs dirs <srv>\n", 0x0C);
        }
        else if (__builtin_strcmp(op, "mkdir") == 0) {
            if (read_token(&cursor, srv, 16) && read_token(&cursor, dir, 16)) command_server_fs_mkdir(srv, dir);
            else print_string("Uso: server fs mkdir <srv> <dir>\n", 0x0C);
        }
        else if (__builtin_strcmp(op, "touch") == 0) {
            if (read_token(&cursor, srv, 16) && read_token(&cursor, dir, 16) && read_token(&cursor, file, 16)) command_server_fs_touch(srv, dir, file);
            else print_string("Uso: server fs touch <srv> <dir> <file>\n", 0x0C);
        }
        else if (__builtin_strcmp(op, "ls") == 0) {
            if (read_token(&cursor, srv, 16) && read_token(&cursor, dir, 16)) command_server_fs_ls(srv, dir);
            else print_string("Uso: server fs ls <srv> <dir>\n", 0x0C);
        }
        else if (__builtin_strcmp(op, "cat") == 0) {
            if (read_token(&cursor, srv, 16) && read_token(&cursor, dir, 16) && read_token(&cursor, file, 16)) command_server_fs_cat(srv, dir, file);
            else print_string("Uso: server fs cat <srv> <dir> <file>\n", 0x0C);
        }
        else if (__builtin_strcmp(op, "write") == 0) {
            const char *txt;
            if (read_token(&cursor, srv, 16) && read_token(&cursor, dir, 16) && read_token(&cursor, file, 16)) {
                txt = skip_spaces(cursor);
                command_server_fs_write(srv, dir, file, txt);
            } else print_string("Uso: server fs write <srv> <dir> <file> <txt>\n", 0x0C);
        }
        else if (__builtin_strcmp(op, "rm") == 0) {
            if (read_token(&cursor, srv, 16) && read_token(&cursor, dir, 16) && read_token(&cursor, file, 16)) command_server_fs_rm(srv, dir, file);
            else print_string("Uso: server fs rm <srv> <dir> <file>\n", 0x0C);
        }
        else if (__builtin_strcmp(op, "rmdir") == 0) {
            if (read_token(&cursor, srv, 16) && read_token(&cursor, dir, 16)) command_server_fs_rmdir(srv, dir);
            else print_string("Uso: server fs rmdir <srv> <dir>\n", 0x0C);
        }
        else if (__builtin_strcmp(op, "chmod") == 0) {
            if (read_token(&cursor, srv, 16) && read_token(&cursor, dir, 16) && read_token(&cursor, file, 16) && read_token(&cursor, mode, 8)) command_server_fs_chmod(srv, dir, file, mode);
            else print_string("Uso: server fs chmod <srv> <dir> <file> <r|w|rw>\n", 0x0C);
        }
        else {
            print_string("Operacion fs no valida\n", 0x0C);
        }
    }
    else if (starts_with(command, "getppid ")) {
        int pid = parse_number(command + 8);
        if (pid >= 0 && pid < MAX_PROCESSES && process_table[pid].state != PROC_EMPTY) {
            char buf[20];
            print_string("PID ", 0x0F);
            int_to_str(pid, buf);
            print_string(buf, 0x0F);
            print_string(" -> Padre: ", 0x0F);
            int_to_str(process_table[pid].parent_pid, buf);
            print_string(buf, 0x0F);
            print_string("\n", 0x0F);
        } else {
            print_string("Error: proceso no existe\n", 0x0C);
        }
    }
    else if (__builtin_strcmp(command, "fork") == 0) {
        int child = create_process("hijo", 0x2000);
        if (child >= 0) {
            char buf[20];
            print_string("Proceso hijo creado: PID ", 0x0A);
            int_to_str(child, buf);
            print_string(buf, 0x0A);
            print_string("\n", 0x0A);
        } else {
            print_string("Error: no se pudo crear proceso\n", 0x0C);
        }
    }
    else if (starts_with(command, "wait ")) {
        int pid = parse_number(command + 5);
        if (pid > 0 && pid < MAX_PROCESSES) {
            print_string("Esperando a proceso ", 0x0B);
            char buf[20];
            int_to_str(pid, buf);
            print_string(buf, 0x0B);
            print_string("...\n", 0x0B);
            int count = 0;
            while (process_table[pid].state != PROC_ZOMBIE && 
                   process_table[pid].state != PROC_EMPTY && count < 1000) {
                count++;
            }
            if (process_table[pid].state == PROC_ZOMBIE) {
                print_string("Proceso terminado\n", 0x0A);
                process_table[pid].state = PROC_EMPTY;
            } else {
                print_string("Timeout esperando proceso\n", 0x0C);
            }
        } else {
            print_string("Error: PID invalido\n", 0x0C);
        }
    }
    else if (__builtin_strcmp(command, "yield") == 0) {
        print_string("Cediendo CPU...\n", 0x0F);
        process_table[current_pid].state = PROC_READY;
    }
    else if (__builtin_strcmp(command, "read") == 0) {
        print_string("Escribe algo: ", 0x0E);
        char buf[80];
        for (int i = 0; i < 79; i++) {
            char c = get_key();
            if (c == '\r') {
                buf[i] = '\0';
                print_char('\n', 0x0E);
                break;
            }
            if (c == '\b' && i > 0) {
                i -= 2;
                print_char('\b', 0x0E);
                continue;
            }
            if (c >= 32 && c < 127) {
                buf[i] = c;
                print_char(c, 0x0E);
            } else {
                i--;
            }
        }
        print_string("Leido: ", 0x0A);
        print_string(buf, 0x0A);
        print_string("\n", 0x0A);
    }
    else if (__builtin_strcmp(command, "stats") == 0) {
        print_string("\n", 0x0F);
        print_string("╔════════════════════════════════════════╗\n", 0x0D);
        print_string("║   ESTADISTICAS DEL SISTEMA OPERATIVO   ║\n", 0x0D);
        print_string("╠════════════════════════════════════════╣\n", 0x0D);
        
        char buf[20];
        int_to_str(timer_ticks / 100, buf);
        print_string("║ Uptime (segundos): ", 0x0F);
        print_string(buf, 0x0F);
        print_string(" s", 0x0F);
        for (int j = 3 + __builtin_strlen(buf); j < 38; j++) print_string(" ", 0x0F);
        print_string("║\n", 0x0F);
        
        int count = 0;
        for (int i = 0; i < MAX_PROCESSES; i++) {
            if (process_table[i].state != PROC_EMPTY) count++;
        }
        print_string("║ Procesos activos: ", 0x0F);
        int_to_str(count, buf);
        print_string(buf, 0x0F);
        print_string(" / ", 0x0F);
        int_to_str(MAX_PROCESSES, buf);
        print_string(buf, 0x0F);
        for (int j = 18 + __builtin_strlen(buf) + __builtin_strlen(buf) + 3; j < 38; j++) print_string(" ", 0x0F);
        print_string("║\n", 0x0F);
        
        print_string("╠════════════════════════════════════════╣\n", 0x0D);
        print_string("║ Estado de Procesos:                    ║\n", 0x0D);
        
        print_string("║   RUNNING: ", 0x0F);
        count = 0;
        for (int i = 0; i < MAX_PROCESSES; i++) {
            if (process_table[i].state == PROC_RUNNING) count++;
        }
        int_to_str(count, buf);
        print_string(buf, 0x0F);
        for (int j = 11 + __builtin_strlen(buf); j < 38; j++) print_string(" ", 0x0F);
        print_string("║\n", 0x0F);
        
        print_string("║   READY:   ", 0x0F);
        count = 0;
        for (int i = 0; i < MAX_PROCESSES; i++) {
            if (process_table[i].state == PROC_READY) count++;
        }
        int_to_str(count, buf);
        print_string(buf, 0x0F);
        for (int j = 11 + __builtin_strlen(buf); j < 38; j++) print_string(" ", 0x0F);
        print_string("║\n", 0x0F);
        
        print_string("║   SLEEP:   ", 0x0F);
        count = 0;
        for (int i = 0; i < MAX_PROCESSES; i++) {
            if (process_table[i].state == PROC_SLEEP) count++;
        }
        int_to_str(count, buf);
        print_string(buf, 0x0F);
        for (int j = 11 + __builtin_strlen(buf); j < 38; j++) print_string(" ", 0x0F);
        print_string("║\n", 0x0F);
        
        print_string("║   ZOMBIE:  ", 0x0F);
        count = 0;
        for (int i = 0; i < MAX_PROCESSES; i++) {
            if (process_table[i].state == PROC_ZOMBIE) count++;
        }
        int_to_str(count, buf);
        print_string(buf, 0x0F);
        for (int j = 11 + __builtin_strlen(buf); j < 38; j++) print_string(" ", 0x0F);
        print_string("║\n", 0x0F);
        
        print_string("╚════════════════════════════════════════╝\n", 0x0D);
        print_string("\n", 0x0F);
    }
    else {
        print_string("Comando no encontrado. Escribe 'help'\n", 0x0C);
    }
}
