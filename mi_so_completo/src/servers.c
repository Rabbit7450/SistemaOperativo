#include "common.h"
#include "servers.h"
#include "vga.h"
#include "utils.h"

ServerService server_table[MAX_SERVERS];
ServerDirectory server_dirs[MAX_SERVERS][MAX_SERVER_DIRS];
ServerFile server_files[MAX_SERVERS][MAX_SERVER_FILES];
ServerSnapshot server_snapshots[MAX_SERVERS];
int server_count = 0;

static char audit_logs[MAX_AUDIT_LOGS][64];
static int audit_count = 0;

#define ROLE_VIEWER 0
#define ROLE_OPERATOR 1
#define ROLE_ADMIN 2

int current_role = ROLE_VIEWER;
int session_authenticated = 0;

static int find_server(const char *name) {
    for (int i = 0; i < server_count; i++) {
        if (__builtin_strcmp(server_table[i].name, name) == 0) {
            return i;
        }
    }
    return -1;
}

static void add_audit_log(const char *text) {
    int idx;
    if (audit_count < MAX_AUDIT_LOGS) {
        idx = audit_count++;
    } else {
        for (int i = 1; i < MAX_AUDIT_LOGS; i++) {
            copy_string(audit_logs[i - 1], audit_logs[i], 64);
        }
        idx = MAX_AUDIT_LOGS - 1;
    }
    copy_string(audit_logs[idx], text, 64);
}

static int fs_dir_exists(int sidx, const char *dir) {
    for (int i = 0; i < MAX_SERVER_DIRS; i++) {
        if (server_dirs[sidx][i].used && __builtin_strcmp(server_dirs[sidx][i].name, dir) == 0) {
            return 1;
        }
    }
    return 0;
}

static int fs_find_file(int sidx, const char *dir, const char *file) {
    for (int i = 0; i < MAX_SERVER_FILES; i++) {
        if (server_files[sidx][i].used &&
            __builtin_strcmp(server_files[sidx][i].dir, dir) == 0 &&
            __builtin_strcmp(server_files[sidx][i].name, file) == 0) {
            return i;
        }
    }
    return -1;
}

static void add_server(const char *name, int active, int cpu, int mem, int health) {
    if (server_count >= MAX_SERVERS) {
        return;
    }
    copy_string(server_table[server_count].name, name, 16);
    server_table[server_count].active = active;
    server_table[server_count].cpu_load = cpu;
    server_table[server_count].memory_mb = mem;
    server_table[server_count].health = health;
    server_table[server_count].restarts = 0;
    server_count++;
}

void init_server_manager() {
    server_count = 0;
    add_server("nginx", 1, 12, 64, 95);
    add_server("postgres", 1, 20, 512, 97);
    add_server("redis", 1, 6, 128, 96);
    add_server("sshd", 1, 2, 32, 99);
    add_server("api-gateway", 0, 0, 96, 88);
    add_server("worker-1", 1, 18, 256, 93);
    add_server("worker-2", 1, 21, 256, 91);
    add_server("worker-3", 0, 0, 256, 90);
}

void init_server_filesystem() {
    for (int s = 0; s < MAX_SERVERS; s++) {
        for (int d = 0; d < MAX_SERVER_DIRS; d++) {
            server_dirs[s][d].used = 0;
            server_dirs[s][d].name[0] = '\0';
        }
        for (int f = 0; f < MAX_SERVER_FILES; f++) {
            server_files[s][f].used = 0;
            server_files[s][f].dir[0] = '\0';
            server_files[s][f].name[0] = '\0';
            server_files[s][f].content[0] = '\0';
            server_files[s][f].perm_read = 1;
            server_files[s][f].perm_write = 1;
        }
        server_snapshots[s].valid = 0;
    }

    audit_count = 0;
    for (int s = 0; s < server_count; s++) {
        server_dirs[s][0].used = 1;
        copy_string(server_dirs[s][0].name, "etc", 16);
        server_dirs[s][1].used = 1;
        copy_string(server_dirs[s][1].name, "logs", 16);
    }
}

void server_tick(int ticks) {
    if (ticks % 100 != 0) {
        return;
    }
    for (int i = 0; i < server_count; i++) {
        if (server_table[i].active) {
            server_table[i].cpu_load = (server_table[i].cpu_load + i + 3) % 60;
            if (server_table[i].cpu_load < 3) {
                server_table[i].cpu_load = 3;
            }
        }
    }
}

const char* role_name(int role) {
    if (role == ROLE_ADMIN) return "admin";
    if (role == ROLE_OPERATOR) return "operator";
    return "viewer";
}

int require_role(int min_role) {
    if (!session_authenticated) {
        print_string("Error: sesion no autenticada. Usa login <rol> <clave>\n", 0x0C);
        return 0;
    }
    if (current_role < min_role) {
        print_string("Error: permisos insuficientes para esta operacion\n", 0x0C);
        return 0;
    }
    return 1;
}

void command_server_list() {
    char buf[16];
    print_string("SERVIDOR      ESTADO CPU MEM  SALUD\n", 0x0B);
    for (int i = 0; i < server_count; i++) {
        print_string(" - ", 0x0F);
        print_string(server_table[i].name, 0x0F);
        print_string(" ", 0x0F);
        print_string(server_table[i].active ? "UP " : "DOWN ", server_table[i].active ? 0x0A : 0x0C);
        int_to_str(server_table[i].cpu_load, buf);
        print_string(buf, 0x0F);
        print_string("% ", 0x0F);
        int_to_str(server_table[i].memory_mb, buf);
        print_string(buf, 0x0F);
        print_string("MB ", 0x0F);
        int_to_str(server_table[i].health, buf);
        print_string(buf, 0x0F);
        print_string("%\n", 0x0F);
    }
}

void command_server_status(const char *name) {
    int sidx = find_server(name);
    char buf[16];
    if (sidx < 0) {
        print_string("Error: servidor no existe\n", 0x0C);
        return;
    }
    print_string("Servidor: ", 0x0E);
    print_string(server_table[sidx].name, 0x0E);
    print_string("\n", 0x0E);
    print_string("Estado: ", 0x0E);
    print_string(server_table[sidx].active ? "UP\n" : "DOWN\n", server_table[sidx].active ? 0x0A : 0x0C);
    print_string("CPU: ", 0x0E);
    int_to_str(server_table[sidx].cpu_load, buf);
    print_string(buf, 0x0E);
    print_string("%\n", 0x0E);
}

void command_server_start(const char *name) {
    if (!require_role(ROLE_OPERATOR)) return;
    int i = find_server(name);
    if (i < 0) { print_string("Error: servidor no existe\n", 0x0C); return; }
    server_table[i].active = 1;
    add_audit_log("server start");
    print_string("Servidor iniciado\n", 0x0A);
}

void command_server_stop(const char *name) {
    if (!require_role(ROLE_OPERATOR)) return;
    int i = find_server(name);
    if (i < 0) { print_string("Error: servidor no existe\n", 0x0C); return; }
    server_table[i].active = 0;
    add_audit_log("server stop");
    print_string("Servidor detenido\n", 0x0A);
}

void command_server_restart(const char *name) {
    if (!require_role(ROLE_OPERATOR)) return;
    int i = find_server(name);
    if (i < 0) { print_string("Error: servidor no existe\n", 0x0C); return; }
    server_table[i].active = 1;
    server_table[i].restarts++;
    add_audit_log("server restart");
    print_string("Servidor reiniciado\n", 0x0A);
}

void command_server_backup(const char *name) {
    if (!require_role(ROLE_OPERATOR)) return;
    int sidx = find_server(name);
    if (sidx < 0) { print_string("Error: servidor no existe\n", 0x0C); return; }
    for (int i = 0; i < MAX_SERVER_DIRS; i++) server_snapshots[sidx].dirs[i] = server_dirs[sidx][i];
    for (int i = 0; i < MAX_SERVER_FILES; i++) server_snapshots[sidx].files[i] = server_files[sidx][i];
    server_snapshots[sidx].valid = 1;
    add_audit_log("backup");
    print_string("Backup completado\n", 0x0A);
}

void command_server_restore(const char *name) {
    if (!require_role(ROLE_ADMIN)) return;
    int sidx = find_server(name);
    if (sidx < 0) { print_string("Error: servidor no existe\n", 0x0C); return; }
    if (!server_snapshots[sidx].valid) { print_string("Error: no existe backup\n", 0x0C); return; }
    for (int i = 0; i < MAX_SERVER_DIRS; i++) server_dirs[sidx][i] = server_snapshots[sidx].dirs[i];
    for (int i = 0; i < MAX_SERVER_FILES; i++) server_files[sidx][i] = server_snapshots[sidx].files[i];
    add_audit_log("restore");
    print_string("Restore completado\n", 0x0A);
}

void command_server_audit() {
    print_string("AUDITORIA:\n", 0x0B);
    for (int i = 0; i < audit_count; i++) {
        print_string(" - ", 0x0F);
        print_string(audit_logs[i], 0x0F);
        print_string("\n", 0x0F);
    }
}

void command_login(const char *role, const char *pass) {
    if (__builtin_strcmp(role, "admin") == 0 && __builtin_strcmp(pass, "admin123") == 0) {
        current_role = ROLE_ADMIN;
        session_authenticated = 1;
        add_audit_log("login admin");
        print_string("Login admin OK\n", 0x0A);
        return;
    }
    if (__builtin_strcmp(role, "operator") == 0 && __builtin_strcmp(pass, "operator123") == 0) {
        current_role = ROLE_OPERATOR;
        session_authenticated = 1;
        add_audit_log("login operator");
        print_string("Login operator OK\n", 0x0A);
        return;
    }
    if (__builtin_strcmp(role, "viewer") == 0 && __builtin_strcmp(pass, "viewer123") == 0) {
        current_role = ROLE_VIEWER;
        session_authenticated = 1;
        add_audit_log("login viewer");
        print_string("Login viewer OK\n", 0x0A);
        return;
    }
    print_string("Error: credenciales invalidas\n", 0x0C);
}

void command_logout() {
    session_authenticated = 0;
    current_role = ROLE_VIEWER;
    add_audit_log("logout");
    print_string("Sesion cerrada\n", 0x0A);
}

void command_server_fs_dirs(const char *srv) {
    int sidx = find_server(srv);
    if (sidx < 0) { print_string("Error: servidor no existe\n", 0x0C); return; }
    print_string("Directorios:\n", 0x0E);
    for (int i = 0; i < MAX_SERVER_DIRS; i++) {
        if (server_dirs[sidx][i].used) {
            print_string(" - ", 0x0F);
            print_string(server_dirs[sidx][i].name, 0x0F);
            print_string("\n", 0x0F);
        }
    }
}

void command_server_fs_mkdir(const char *srv, const char *dir) {
    if (!require_role(ROLE_OPERATOR)) return;
    int sidx = find_server(srv);
    if (sidx < 0 || !server_table[sidx].active) { print_string("Error: servidor invalido o detenido\n", 0x0C); return; }
    if (fs_dir_exists(sidx, dir)) { print_string("Directorio ya existe\n", 0x0E); return; }
    for (int i = 0; i < MAX_SERVER_DIRS; i++) {
        if (!server_dirs[sidx][i].used) {
            server_dirs[sidx][i].used = 1;
            copy_string(server_dirs[sidx][i].name, dir, 16);
            add_audit_log("mkdir");
            print_string("Directorio creado\n", 0x0A);
            return;
        }
    }
    print_string("Error: sin espacio para mas directorios\n", 0x0C);
}

void command_server_fs_touch(const char *srv, const char *dir, const char *file) {
    if (!require_role(ROLE_OPERATOR)) return;
    int sidx = find_server(srv);
    if (sidx < 0 || !server_table[sidx].active || !fs_dir_exists(sidx, dir)) { print_string("Error: servidor/directorio invalido\n", 0x0C); return; }
    if (fs_find_file(sidx, dir, file) >= 0) { print_string("Archivo ya existe\n", 0x0E); return; }
    for (int i = 0; i < MAX_SERVER_FILES; i++) {
        if (!server_files[sidx][i].used) {
            server_files[sidx][i].used = 1;
            copy_string(server_files[sidx][i].dir, dir, 16);
            copy_string(server_files[sidx][i].name, file, 16);
            server_files[sidx][i].perm_read = 1;
            server_files[sidx][i].perm_write = 1;
            add_audit_log("touch");
            print_string("Archivo creado\n", 0x0A);
            return;
        }
    }
    print_string("Error: sin espacio para mas archivos\n", 0x0C);
}

void command_server_fs_ls(const char *srv, const char *dir) {
    int sidx = find_server(srv);
    if (sidx < 0 || !fs_dir_exists(sidx, dir)) { print_string("Error: servidor/directorio invalido\n", 0x0C); return; }
    for (int i = 0; i < MAX_SERVER_FILES; i++) {
        if (server_files[sidx][i].used && __builtin_strcmp(server_files[sidx][i].dir, dir) == 0) {
            print_string(" - ", 0x0F);
            print_string(server_files[sidx][i].name, 0x0F);
            print_string("\n", 0x0F);
        }
    }
}

void command_server_fs_write(const char *srv, const char *dir, const char *file, const char *content) {
    if (!require_role(ROLE_OPERATOR)) return;
    int sidx = find_server(srv);
    if (sidx < 0 || !server_table[sidx].active) { print_string("Error: servidor invalido o detenido\n", 0x0C); return; }
    int fidx = fs_find_file(sidx, dir, file);
    if (fidx < 0 || !server_files[sidx][fidx].perm_write) { print_string("Error: archivo invalido o sin permiso write\n", 0x0C); return; }
    copy_string(server_files[sidx][fidx].content, content, MAX_FILE_CONTENT);
    add_audit_log("write");
    print_string("Contenido actualizado\n", 0x0A);
}

void command_server_fs_cat(const char *srv, const char *dir, const char *file) {
    int sidx = find_server(srv);
    if (sidx < 0) { print_string("Error: servidor no existe\n", 0x0C); return; }
    int fidx = fs_find_file(sidx, dir, file);
    if (fidx < 0 || !server_files[sidx][fidx].perm_read) { print_string("Error: archivo invalido o sin permiso read\n", 0x0C); return; }
    print_string("Contenido: ", 0x0E);
    print_string(server_files[sidx][fidx].content[0] ? server_files[sidx][fidx].content : "(vacio)", 0x0F);
    print_string("\n", 0x0F);
}

void command_server_fs_rm(const char *srv, const char *dir, const char *file) {
    if (!require_role(ROLE_ADMIN)) return;
    int sidx = find_server(srv);
    if (sidx < 0) { print_string("Error: servidor no existe\n", 0x0C); return; }
    int fidx = fs_find_file(sidx, dir, file);
    if (fidx < 0) { print_string("Error: archivo no existe\n", 0x0C); return; }
    server_files[sidx][fidx].used = 0;
    add_audit_log("rm");
    print_string("Archivo eliminado\n", 0x0A);
}

void command_server_fs_rmdir(const char *srv, const char *dir) {
    if (!require_role(ROLE_ADMIN)) return;
    int sidx = find_server(srv);
    if (sidx < 0) { print_string("Error: servidor no existe\n", 0x0C); return; }
    for (int i = 0; i < MAX_SERVER_FILES; i++) {
        if (server_files[sidx][i].used && __builtin_strcmp(server_files[sidx][i].dir, dir) == 0) {
            print_string("Error: directorio no vacio\n", 0x0C);
            return;
        }
    }
    for (int i = 0; i < MAX_SERVER_DIRS; i++) {
        if (server_dirs[sidx][i].used && __builtin_strcmp(server_dirs[sidx][i].name, dir) == 0) {
            server_dirs[sidx][i].used = 0;
            add_audit_log("rmdir");
            print_string("Directorio eliminado\n", 0x0A);
            return;
        }
    }
}

void command_server_fs_chmod(const char *srv, const char *dir, const char *file, const char *mode) {
    if (!require_role(ROLE_ADMIN)) return;
    int sidx = find_server(srv);
    if (sidx < 0) { print_string("Error: servidor no existe\n", 0x0C); return; }
    int fidx = fs_find_file(sidx, dir, file);
    if (fidx < 0) { print_string("Error: archivo no existe\n", 0x0C); return; }
    if (__builtin_strcmp(mode, "r") == 0) {
        server_files[sidx][fidx].perm_read = 1;
        server_files[sidx][fidx].perm_write = 0;
    } else if (__builtin_strcmp(mode, "w") == 0) {
        server_files[sidx][fidx].perm_read = 0;
        server_files[sidx][fidx].perm_write = 1;
    } else if (__builtin_strcmp(mode, "rw") == 0) {
        server_files[sidx][fidx].perm_read = 1;
        server_files[sidx][fidx].perm_write = 1;
    } else {
        print_string("Modo invalido\n", 0x0C);
        return;
    }
    add_audit_log("chmod");
    print_string("Permisos actualizados\n", 0x0A);
}

void command_principales() {
    print_string("Sistema: help cmd clear about uptime memory whoami login logout\n", 0x0F);
    print_string("Procesos: ps top exec runu kill sleep priority\n", 0x0F);
    print_string("Servidores: list status start stop restart backup restore audit\n", 0x0F);
    print_string("FS: dirs mkdir touch ls write cat rm rmdir chmod\n", 0x0F);
}
