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
        print_string("Comandos del SO v3.2 modular:\n", 0x0A);
        print_string("  help clear about uptime memory\n", 0x0A);
        print_string("  whoami login logout cmd principales\n", 0x0A);
        print_string("  ps top exec runu kill sleep priority\n", 0x0A);
        print_string("  add echo\n", 0x0A);
        print_string("  server list|status|start|stop|restart|backup|restore|audit\n", 0x0A);
        print_string("  server fs dirs|mkdir|touch|ls|write|cat|rm|rmdir|chmod\n", 0x0A);
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
        print_string("PID  NOMBRE            ESTADO    MEM(KB)\n", 0x0B);
        for (int i = 0; i < MAX_PROCESSES; i++) {
            if (process_table[i].state != PROC_EMPTY) {
                char buf[20];
                int_to_str(process_table[i].pid, buf);
                print_string(buf, 0x0F);
                print_string(" ", 0x0F);
                print_string(process_table[i].name, 0x0F);
                print_string(" ", 0x0F);
                switch (process_table[i].state) {
                    case PROC_RUNNING: print_string("RUN", 0x0A); break;
                    case PROC_READY: print_string("READY", 0x0B); break;
                    case PROC_SLEEP: print_string("SLEEP", 0x0E); break;
                    case PROC_ZOMBIE: print_string("ZOMB", 0x0C); break;
                    default: print_string("?", 0x0F); break;
                }
                print_string("\n", 0x0F);
            }
        }
    }
    else if (__builtin_strcmp(command, "top") == 0) {
        print_string("PID  NOMBRE       ST     PRIO  MEM(KB) CPU\n", 0x0B);
        for (int i = 0; i < MAX_PROCESSES; i++) {
            if (process_table[i].state != PROC_EMPTY) {
                char buf[20];
                int_to_str(process_table[i].pid, buf);
                print_string(buf, 0x0F);
                print_string("   ", 0x0F);
                print_string(process_table[i].name, 0x0F);
                print_string(" ", 0x0F);
                switch (process_table[i].state) {
                    case PROC_RUNNING: print_string("RUN   ", 0x0A); break;
                    case PROC_READY: print_string("READY ", 0x0B); break;
                    case PROC_SLEEP: print_string("SLEEP ", 0x0E); break;
                    case PROC_ZOMBIE: print_string("ZOMB  ", 0x0C); break;
                    default: print_string("?     ", 0x0F); break;
                }
                int_to_str(process_table[i].priority, buf);
                print_string(buf, 0x0F);
                print_string("     ", 0x0F);
                int_to_str(process_table[i].memory_size / 1024, buf);
                print_string(buf, 0x0F);
                print_string("      ", 0x0F);
                int_to_str(process_table[i].cpu_time, buf);
                print_string(buf, 0x0F);
                print_string("\n", 0x0F);
            }
        }
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
    else {
        print_string("Comando no encontrado. Escribe 'help'\n", 0x0C);
    }
}
