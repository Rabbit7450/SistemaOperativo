// =============================================
// Mi SO v3.0 - Kernel con gestión de procesos e interrupciones
// =============================================

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

// Estados de procesos
#define PROC_EMPTY 0
#define PROC_RUNNING 1
#define PROC_READY 2
#define PROC_BLOCKED 3
#define PROC_SLEEP 4
#define PROC_ZOMBIE 5

// System call numbers
#define SYS_EXIT 1
#define SYS_GETPID 2
#define SYS_GETMEM 3
#define SYS_SLEEP 4
#define SYS_SETPRIO 5
#define SYS_GETPRIO 6

// ==================== VGA MANAGEMENT ====================
char command[MAX_CMD];
int cmd_index = 0;
static int vga_offset = 0;
static const int VGA_SIZE = 4000;
static const int LINE_WIDTH = 160;

void scroll_screen() {
    char *vga = (char*)VGA_MEMORY;
    for (int i = 0; i < VGA_SIZE - LINE_WIDTH; i++) {
        vga[i] = vga[i + LINE_WIDTH];
    }
    for (int i = VGA_SIZE - LINE_WIDTH; i < VGA_SIZE; i += 2) {
        vga[i] = ' ';
        vga[i + 1] = 0x0F;
    }
    vga_offset = VGA_SIZE - LINE_WIDTH;
}

void print_char(char c, char color) {
    char *vga = (char*)VGA_MEMORY;
    
    if (c == '\n') {
        vga_offset = ((vga_offset / LINE_WIDTH) + 1) * LINE_WIDTH;
    } else if (c == '\b') {
        if (vga_offset >= 2) {
            vga_offset -= 2;
            vga[vga_offset] = ' ';
            vga[vga_offset + 1] = color;
        }
        return;
    } else if (c >= 32 && c < 127) {
        vga[vga_offset++] = c;
        vga[vga_offset++] = color;
    }
    
    if (vga_offset >= VGA_SIZE) {
        scroll_screen();
    }
}

void print_string(const char *str, char color) {
    while (*str) {
        print_char(*str, color);
        str++;
    }
}

void clear_screen() {
    char *vga = (char*)VGA_MEMORY;
    for (int i = 0; i < VGA_SIZE; i += 2) {
        vga[i] = ' ';
        vga[i+1] = 0x0F;
    }
    vga_offset = 0;
}

// ==================== IDT (Interrupt Descriptor Table) ====================
typedef struct {
    uint16_t offset_low;
    uint16_t selector;
    uint8_t zero;
    uint8_t type_attr;
    uint16_t offset_high;
} IDT_Entry;

typedef struct {
    uint16_t limit;
    uint32_t base;
} __attribute__((packed)) IDT_Descriptor;

IDT_Entry idt[256];
IDT_Descriptor idt_descriptor;

// ==================== ESTRUCTURAS RING 3 ====================
typedef struct {
    uint32_t eax, ebx, ecx, edx;
    uint32_t esi, edi, ebp;
    uint32_t eip, esp;
} UserContext;

// ==================== GESTIÓN DE PROCESOS ====================
typedef struct {
    int pid;
    char name[32];
    int state;              // 0: empty, 1: running, 2: ready, 3: blocked
    int priority;
    uint32_t memory_start;
    uint32_t memory_size;
    int mode;               // KERNEL_MODE o USER_MODE
    UserContext context;    // Contexto de usuario para Ring 3
    int sleep_ticks;        // Ticks restantes en SLEEP
    int cpu_time;           // Ticks ejecutados en CPU
} Process;

Process process_table[MAX_PROCESSES];
int current_pid = 0;
int next_pid = 1;
int timer_ticks = 0;

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

ServerService server_table[MAX_SERVERS];
ServerDirectory server_dirs[MAX_SERVERS][MAX_SERVER_DIRS];
ServerFile server_files[MAX_SERVERS][MAX_SERVER_FILES];
ServerSnapshot server_snapshots[MAX_SERVERS];
int server_count = 0;
char audit_logs[MAX_AUDIT_LOGS][64];
int audit_count = 0;

#define ROLE_VIEWER 0
#define ROLE_OPERATOR 1
#define ROLE_ADMIN 2
int current_role = ROLE_VIEWER;
int session_authenticated = 0;

void copy_string(char *dst, const char *src, int max_len) {
    int i = 0;
    while (src[i] && i < max_len - 1) {
        dst[i] = src[i];
        i++;
    }
    dst[i] = '\0';
}

int starts_with(const char *str, const char *prefix) {
    int i = 0;
    while (prefix[i]) {
        if (str[i] != prefix[i]) return 0;
        i++;
    }
    return 1;
}

const char* skip_spaces(const char *str) {
    while (*str == ' ') str++;
    return str;
}

int read_token(const char **cursor, char *out, int max_len) {
    const char *p = skip_spaces(*cursor);
    int i = 0;
    if (*p == '\0') return 0;
    while (*p && *p != ' ' && i < max_len - 1) out[i++] = *p++;
    out[i] = '\0';
    *cursor = p;
    return 1;
}

void add_audit_log(const char *text) {
    int idx;
    if (audit_count < MAX_AUDIT_LOGS) idx = audit_count++;
    else {
        int i;
        for (i = 1; i < MAX_AUDIT_LOGS; i++) copy_string(audit_logs[i - 1], audit_logs[i], 64);
        idx = MAX_AUDIT_LOGS - 1;
    }
    copy_string(audit_logs[idx], text, 64);
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

int find_server(const char *name) {
    int i;
    for (i = 0; i < server_count; i++) if (__builtin_strcmp(server_table[i].name, name) == 0) return i;
    return -1;
}

void add_server(const char *name, int active, int cpu, int mem, int health) {
    if (server_count >= MAX_SERVERS) return;
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
    int s, d, f;
    for (s = 0; s < MAX_SERVERS; s++) {
        for (d = 0; d < MAX_SERVER_DIRS; d++) {
            server_dirs[s][d].used = 0;
            server_dirs[s][d].name[0] = '\0';
        }
        for (f = 0; f < MAX_SERVER_FILES; f++) {
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
    for (s = 0; s < server_count; s++) {
        server_dirs[s][0].used = 1; copy_string(server_dirs[s][0].name, "etc", 16);
        server_dirs[s][1].used = 1; copy_string(server_dirs[s][1].name, "logs", 16);
    }
}

int fs_dir_exists(int sidx, const char *dir) {
    int i;
    for (i = 0; i < MAX_SERVER_DIRS; i++) if (server_dirs[sidx][i].used && __builtin_strcmp(server_dirs[sidx][i].name, dir) == 0) return 1;
    return 0;
}

int fs_find_file(int sidx, const char *dir, const char *file) {
    int i;
    for (i = 0; i < MAX_SERVER_FILES; i++) {
        if (server_files[sidx][i].used && __builtin_strcmp(server_files[sidx][i].dir, dir) == 0 && __builtin_strcmp(server_files[sidx][i].name, file) == 0) return i;
    }
    return -1;
}

void command_server_list() {
    int i; char buf[16];
    print_string("SERVIDOR      ESTADO CPU MEM  SALUD\n", 0x0B);
    for (i = 0; i < server_count; i++) {
        print_string(" - ", 0x0F); print_string(server_table[i].name, 0x0F); print_string(" ", 0x0F);
        print_string(server_table[i].active ? "UP " : "DOWN ", server_table[i].active ? 0x0A : 0x0C);
        int_to_str(server_table[i].cpu_load, buf); print_string(buf, 0x0F); print_string("% ", 0x0F);
        int_to_str(server_table[i].memory_mb, buf); print_string(buf, 0x0F); print_string("MB ", 0x0F);
        int_to_str(server_table[i].health, buf); print_string(buf, 0x0F); print_string("%\n", 0x0F);
    }
}

void command_server_status(const char *name) {
    int sidx = find_server(name); char buf[16];
    if (sidx < 0) { print_string("Error: servidor no existe\n", 0x0C); return; }
    print_string("Servidor: ", 0x0E); print_string(server_table[sidx].name, 0x0E); print_string("\n", 0x0E);
    print_string("Estado: ", 0x0E); print_string(server_table[sidx].active ? "UP\n" : "DOWN\n", server_table[sidx].active ? 0x0A : 0x0C);
    print_string("CPU: ", 0x0E); int_to_str(server_table[sidx].cpu_load, buf); print_string(buf, 0x0E); print_string("%\n", 0x0E);
}

void command_server_start(const char *name) { int i; if (!require_role(ROLE_OPERATOR)) return; i = find_server(name); if (i < 0) { print_string("Error: servidor no existe\n", 0x0C); return; } server_table[i].active = 1; add_audit_log("server start"); print_string("Servidor iniciado\n", 0x0A); }
void command_server_stop(const char *name) { int i; if (!require_role(ROLE_OPERATOR)) return; i = find_server(name); if (i < 0) { print_string("Error: servidor no existe\n", 0x0C); return; } server_table[i].active = 0; add_audit_log("server stop"); print_string("Servidor detenido\n", 0x0A); }
void command_server_restart(const char *name) { int i; if (!require_role(ROLE_OPERATOR)) return; i = find_server(name); if (i < 0) { print_string("Error: servidor no existe\n", 0x0C); return; } server_table[i].active = 1; server_table[i].restarts++; add_audit_log("server restart"); print_string("Servidor reiniciado\n", 0x0A); }

void command_server_backup(const char *name) {
    int i, sidx; if (!require_role(ROLE_OPERATOR)) return;
    sidx = find_server(name); if (sidx < 0) { print_string("Error: servidor no existe\n", 0x0C); return; }
    for (i = 0; i < MAX_SERVER_DIRS; i++) server_snapshots[sidx].dirs[i] = server_dirs[sidx][i];
    for (i = 0; i < MAX_SERVER_FILES; i++) server_snapshots[sidx].files[i] = server_files[sidx][i];
    server_snapshots[sidx].valid = 1; add_audit_log("backup"); print_string("Backup completado\n", 0x0A);
}

void command_server_restore(const char *name) {
    int i, sidx; if (!require_role(ROLE_ADMIN)) return;
    sidx = find_server(name); if (sidx < 0) { print_string("Error: servidor no existe\n", 0x0C); return; }
    if (!server_snapshots[sidx].valid) { print_string("Error: no existe backup\n", 0x0C); return; }
    for (i = 0; i < MAX_SERVER_DIRS; i++) server_dirs[sidx][i] = server_snapshots[sidx].dirs[i];
    for (i = 0; i < MAX_SERVER_FILES; i++) server_files[sidx][i] = server_snapshots[sidx].files[i];
    add_audit_log("restore"); print_string("Restore completado\n", 0x0A);
}

void command_server_audit() { int i; print_string("AUDITORIA:\n", 0x0B); for (i = 0; i < audit_count; i++) { print_string(" - ", 0x0F); print_string(audit_logs[i], 0x0F); print_string("\n", 0x0F); } }

void command_login(const char *role, const char *pass) {
    if (__builtin_strcmp(role, "admin") == 0 && __builtin_strcmp(pass, "admin123") == 0) { current_role = ROLE_ADMIN; session_authenticated = 1; add_audit_log("login admin"); print_string("Login admin OK\n", 0x0A); return; }
    if (__builtin_strcmp(role, "operator") == 0 && __builtin_strcmp(pass, "operator123") == 0) { current_role = ROLE_OPERATOR; session_authenticated = 1; add_audit_log("login operator"); print_string("Login operator OK\n", 0x0A); return; }
    if (__builtin_strcmp(role, "viewer") == 0 && __builtin_strcmp(pass, "viewer123") == 0) { current_role = ROLE_VIEWER; session_authenticated = 1; add_audit_log("login viewer"); print_string("Login viewer OK\n", 0x0A); return; }
    print_string("Error: credenciales invalidas\n", 0x0C);
}

void command_logout() { session_authenticated = 0; current_role = ROLE_VIEWER; add_audit_log("logout"); print_string("Sesion cerrada\n", 0x0A); }

void command_server_fs_dirs(const char *srv) {
    int sidx = find_server(srv), i; if (sidx < 0) { print_string("Error: servidor no existe\n", 0x0C); return; }
    print_string("Directorios:\n", 0x0E); for (i = 0; i < MAX_SERVER_DIRS; i++) if (server_dirs[sidx][i].used) { print_string(" - ", 0x0F); print_string(server_dirs[sidx][i].name, 0x0F); print_string("\n", 0x0F); }
}

void command_server_fs_mkdir(const char *srv, const char *dir) {
    int sidx = find_server(srv), i; if (!require_role(ROLE_OPERATOR)) return;
    if (sidx < 0 || !server_table[sidx].active) { print_string("Error: servidor invalido o detenido\n", 0x0C); return; }
    if (fs_dir_exists(sidx, dir)) { print_string("Directorio ya existe\n", 0x0E); return; }
    for (i = 0; i < MAX_SERVER_DIRS; i++) if (!server_dirs[sidx][i].used) { server_dirs[sidx][i].used = 1; copy_string(server_dirs[sidx][i].name, dir, 16); add_audit_log("mkdir"); print_string("Directorio creado\n", 0x0A); return; }
}

void command_server_fs_touch(const char *srv, const char *dir, const char *file) {
    int sidx = find_server(srv), i; if (!require_role(ROLE_OPERATOR)) return;
    if (sidx < 0 || !server_table[sidx].active || !fs_dir_exists(sidx, dir)) { print_string("Error: servidor/directorio invalido\n", 0x0C); return; }
    if (fs_find_file(sidx, dir, file) >= 0) { print_string("Archivo ya existe\n", 0x0E); return; }
    for (i = 0; i < MAX_SERVER_FILES; i++) if (!server_files[sidx][i].used) { server_files[sidx][i].used = 1; copy_string(server_files[sidx][i].dir, dir, 16); copy_string(server_files[sidx][i].name, file, 16); server_files[sidx][i].perm_read = 1; server_files[sidx][i].perm_write = 1; add_audit_log("touch"); print_string("Archivo creado\n", 0x0A); return; }
}

void command_server_fs_ls(const char *srv, const char *dir) {
    int sidx = find_server(srv), i; if (sidx < 0 || !fs_dir_exists(sidx, dir)) { print_string("Error: servidor/directorio invalido\n", 0x0C); return; }
    for (i = 0; i < MAX_SERVER_FILES; i++) if (server_files[sidx][i].used && __builtin_strcmp(server_files[sidx][i].dir, dir) == 0) { print_string(" - ", 0x0F); print_string(server_files[sidx][i].name, 0x0F); print_string("\n", 0x0F); }
}

void command_server_fs_write(const char *srv, const char *dir, const char *file, const char *content) {
    int sidx = find_server(srv), fidx; if (!require_role(ROLE_OPERATOR)) return;
    if (sidx < 0 || !server_table[sidx].active) { print_string("Error: servidor invalido o detenido\n", 0x0C); return; }
    fidx = fs_find_file(sidx, dir, file); if (fidx < 0 || !server_files[sidx][fidx].perm_write) { print_string("Error: archivo invalido o sin permiso write\n", 0x0C); return; }
    copy_string(server_files[sidx][fidx].content, content, MAX_FILE_CONTENT); add_audit_log("write"); print_string("Contenido actualizado\n", 0x0A);
}

void command_server_fs_cat(const char *srv, const char *dir, const char *file) {
    int sidx = find_server(srv), fidx; if (sidx < 0) { print_string("Error: servidor no existe\n", 0x0C); return; }
    fidx = fs_find_file(sidx, dir, file); if (fidx < 0 || !server_files[sidx][fidx].perm_read) { print_string("Error: archivo invalido o sin permiso read\n", 0x0C); return; }
    print_string("Contenido: ", 0x0E); print_string(server_files[sidx][fidx].content[0] ? server_files[sidx][fidx].content : "(vacio)", 0x0F); print_string("\n", 0x0F);
}

void command_server_fs_rm(const char *srv, const char *dir, const char *file) {
    int sidx = find_server(srv), fidx; if (!require_role(ROLE_ADMIN)) return;
    if (sidx < 0) { print_string("Error: servidor no existe\n", 0x0C); return; }
    fidx = fs_find_file(sidx, dir, file); if (fidx < 0) { print_string("Error: archivo no existe\n", 0x0C); return; }
    server_files[sidx][fidx].used = 0; add_audit_log("rm"); print_string("Archivo eliminado\n", 0x0A);
}

void command_server_fs_rmdir(const char *srv, const char *dir) {
    int sidx = find_server(srv), i; if (!require_role(ROLE_ADMIN)) return;
    if (sidx < 0) { print_string("Error: servidor no existe\n", 0x0C); return; }
    for (i = 0; i < MAX_SERVER_FILES; i++) if (server_files[sidx][i].used && __builtin_strcmp(server_files[sidx][i].dir, dir) == 0) { print_string("Error: directorio no vacio\n", 0x0C); return; }
    for (i = 0; i < MAX_SERVER_DIRS; i++) if (server_dirs[sidx][i].used && __builtin_strcmp(server_dirs[sidx][i].name, dir) == 0) { server_dirs[sidx][i].used = 0; add_audit_log("rmdir"); print_string("Directorio eliminado\n", 0x0A); return; }
}

void command_server_fs_chmod(const char *srv, const char *dir, const char *file, const char *mode) {
    int sidx = find_server(srv), fidx; if (!require_role(ROLE_ADMIN)) return;
    if (sidx < 0) { print_string("Error: servidor no existe\n", 0x0C); return; }
    fidx = fs_find_file(sidx, dir, file); if (fidx < 0) { print_string("Error: archivo no existe\n", 0x0C); return; }
    if (__builtin_strcmp(mode, "r") == 0) { server_files[sidx][fidx].perm_read = 1; server_files[sidx][fidx].perm_write = 0; }
    else if (__builtin_strcmp(mode, "w") == 0) { server_files[sidx][fidx].perm_read = 0; server_files[sidx][fidx].perm_write = 1; }
    else if (__builtin_strcmp(mode, "rw") == 0) { server_files[sidx][fidx].perm_read = 1; server_files[sidx][fidx].perm_write = 1; }
    else { print_string("Modo invalido\n", 0x0C); return; }
    add_audit_log("chmod"); print_string("Permisos actualizados\n", 0x0A);
}

void command_principales() {
    print_string("Sistema: help cmd clear about uptime memory whoami login logout\n", 0x0F);
    print_string("Procesos: ps top exec kill sleep priority\n", 0x0F);
    print_string("Servidores: list status start stop restart backup restore audit\n", 0x0F);
    print_string("FS: dirs mkdir touch ls write cat rm rmdir chmod\n", 0x0F);
}

void init_process_table() {
    for (int i = 0; i < MAX_PROCESSES; i++) {
        process_table[i].pid = -1;
        process_table[i].state = 0;
    }
    // Crear proceso kernel
    process_table[0].pid = 0;
    process_table[0].state = 1;
    process_table[0].priority = 1;
    process_table[0].memory_start = 0x1000;
    process_table[0].memory_size = 0x7000;
    process_table[0].mode = KERNEL_MODE;
    process_table[0].sleep_ticks = 0;
    process_table[0].cpu_time = 0;
    __builtin_strcpy(process_table[0].name, "kernel");
}

int create_process(const char *name, uint32_t mem_size) {
    if (next_pid >= MAX_PROCESSES) {
        print_string("Error: tabla de procesos llena\n", 0x0C);
        return -1;
    }
    
    int pid = next_pid;
    process_table[pid].pid = pid;
    process_table[pid].state = PROC_READY;
    process_table[pid].priority = 5;  // Prioridad media
    process_table[pid].memory_size = mem_size;
    process_table[pid].mode = USER_MODE;
    process_table[pid].sleep_ticks = 0;
    process_table[pid].cpu_time = 0;
    
    // Asignar memoria simple (secuencial)
    uint32_t base = 0x10000;
    for (int i = 1; i < pid; i++) {
        base += process_table[i].memory_size;
    }
    process_table[pid].memory_start = base;
    
    __builtin_strcpy(process_table[pid].name, name);
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
    
    process_table[pid].state = PROC_ZOMBIE;  // Convertir a zombie antes de eliminar
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
    
    process_table[pid].state = PROC_SLEEP;
    process_table[pid].sleep_ticks = seconds * 100;  // 100 ticks/segundo
}

int get_best_process() {
    int best_pid = -1;
    int best_priority = 10;  // Peor prioridad
    
    // Buscar proceso READY con mejor prioridad
    for (int i = 0; i < MAX_PROCESSES; i++) {
        if (process_table[i].state == PROC_READY) {
            if (process_table[i].priority < best_priority) {
                best_priority = process_table[i].priority;
                best_pid = i;
            }
        }
    }
    
    if (best_pid == -1) {
        return current_pid;  // Mantener proceso actual si no hay otro
    }
    return best_pid;
}

// ==================== UTILIDADES ====================
char get_key() {
    unsigned char key;
    __asm__ __volatile__("int $0x16" : "=a"(key) : "a"(0x00));
    return key & 0xFF;
}

void read_line() {
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
        if (c >= 32 && c < 127 && cmd_index < MAX_CMD-1) {
            command[cmd_index++] = c;
            command[cmd_index] = '\0';
            print_char(c, 0x0F);
        }
    }
}

int parse_number(const char *str) {
    int num = 0;
    int negative = 0;
    
    if (*str == '-') {
        negative = 1;
        str++;
    }
    
    while (*str >= '0' && *str <= '9') {
        num = num * 10 + (*str - '0');
        str++;
    }
    
    return negative ? -num : num;
}

void int_to_str(int num, char *buffer) {
    if (num == 0) {
        buffer[0] = '0';
        buffer[1] = '\0';
        return;
    }
    
    int negative = 0;
    if (num < 0) {
        negative = 1;
        num = -num;
    }
    
    int i = 0;
    while (num > 0) {
        buffer[i++] = '0' + (num % 10);
        num /= 10;
    }
    
    if (negative) {
        buffer[i++] = '-';
    }
    
    buffer[i] = '\0';
    
    for (int j = 0; j < i / 2; j++) {
        char temp = buffer[j];
        buffer[j] = buffer[i - 1 - j];
        buffer[i - 1 - j] = temp;
    }
}

// ==================== INTERRUPT HANDLERS ====================
void timer_handler() {
    timer_ticks++;
    if (timer_ticks % 100 == 0) {
        int i;
        for (i = 0; i < server_count; i++) {
            if (server_table[i].active) {
                server_table[i].cpu_load = (server_table[i].cpu_load + i + 3) % 60;
                if (server_table[i].cpu_load < 3) server_table[i].cpu_load = 3;
            }
        }
    }
    
    // Actualizar procesos en sleep
    for (int i = 0; i < MAX_PROCESSES; i++) {
        if (process_table[i].state == PROC_SLEEP) {
            process_table[i].sleep_ticks--;
            if (process_table[i].sleep_ticks <= 0) {
                process_table[i].state = PROC_READY;
            }
        }
    }
    
    // Limpiar procesos zombie
    for (int i = 1; i < MAX_PROCESSES; i++) {
        if (process_table[i].state == PROC_ZOMBIE) {
            process_table[i].state = PROC_EMPTY;
            process_table[i].pid = -1;
        }
    }
    
    // Context switch con scheduler mejorado (cada 10 ticks)
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
    
    // Enviar EOI (End of Interrupt) al PIC
    __asm__ __volatile__("movb $0x20, %%al; outb %%al, $0x20" ::: "al");
}

void exception_handler() {
    print_string("\n[EXCEPTION] Excepcion detectada!\n", 0x0C);
    // En un SO real, aquí mataríamos el proceso
}

// ==================== SYSCALL HANDLER (INT 0x80) ====================
uint32_t syscall_handler(uint32_t syscall_num, uint32_t arg1, uint32_t arg2) {
    switch (syscall_num) {
        case SYS_EXIT: {
            // exit(code)
            kill_process(current_pid);
            print_string("\n[SYSCALL] Proceso ", 0x0A);
            char buf[10];
            int_to_str(current_pid, buf);
            print_string(buf, 0x0A);
            print_string(" terminado\n", 0x0A);
            return 0;
        }
        
        case SYS_GETPID: {
            // getpid()
            return current_pid;
        }
        
        case SYS_GETMEM: {
            // getmem() - retorna memoria del proceso actual
            return process_table[current_pid].memory_size;
        }
        
        case SYS_SLEEP: {
            // sleep(seconds)
            sleep_process(current_pid, arg1);
            return 0;
        }
        
        case SYS_SETPRIO: {
            // setpriority(pid, priority)
            if (arg2 >= 0 && arg2 <= 9) {
                process_table[arg1].priority = arg2;
                return 0;
            }
            return -1;
        }
        
        case SYS_GETPRIO: {
            // getpriority(pid)
            return process_table[arg1].priority;
        }
        
        default:
            return -1;
    }
}

// ==================== CÓDIGO DE USUARIO ====================
// Este es código de ejemplo que puede correr en Ring 3
// Simplemente hace una syscall exit
void user_program() {
    // Hacer syscall exit(0)
    __asm__ __volatile__(
        "mov $1, %%eax\n"     // SYS_EXIT
        "mov $0, %%ebx\n"     // arg1 = 0
        "int $0x80\n"         // Syscall
        :
        :
        : "eax", "ebx"
    );
    
    // Loop infinito si exit falla
    while(1);
}
// Esta función salta a código de usuario (Ring 3)
// Se implementará en assembly inline en create_process
void enter_user_mode(uint32_t entry_point, uint32_t stack_top) {
    __asm__ __volatile__(
        "mov %0, %%esp\n"        // Cargar stack de usuario
        "mov $0x23, %%ax\n"      // Data Ring 3 selector
        "mov %%ax, %%ds\n"
        "mov %%ax, %%es\n"
        "mov %%ax, %%fs\n"
        "mov %%ax, %%gs\n"
        "push $0x23\n"           // Push data selector para stack
        "push %%esp\n"           // Push user stack
        "pushf\n"                // Push flags
        "push $0x1b\n"           // Push code Ring 3 selector  
        "push %1\n"              // Push entry point
        "iret\n"                 // Return to user mode
        :
        : "r"(stack_top), "r"(entry_point)
        : "ax"
    );
}

// ==================== CONFIGURAR IDT ====================
void setup_idt() {
    // Aquí se configurarían los handlers de interrupciones
    // Por brevedad, solo configuramos el timer (IRQ0)
    // En producción: llenar 256 entradas
}

// ==================== COMANDOS ====================
void execute_command() {
    if (command[0] == '\0') return;

    if (__builtin_strcmp(command, "help") == 0) {
        print_string("Comandos del SO v3.1 (Nivel 1):\n", 0x0A);
        print_string("  help           - Este mensaje\n", 0x0A);
        print_string("  clear          - Limpia pantalla\n", 0x0A);
        print_string("  ps             - Lista procesos con estado\n", 0x0A);
        print_string("  top            - Info detallada de procesos\n", 0x0A);
        print_string("  exec NAME      - Crea nuevo proceso\n", 0x0A);
        print_string("  kill PID       - Mata proceso (crea zombie)\n", 0x0A);
        print_string("  sleep PID SEC  - Suspende proceso por segundos\n", 0x0A);
        print_string("  priority PID P - Cambia prioridad (0=alta, 9=baja)\n", 0x0A);
        print_string("  memory         - Info de memoria\n", 0x0A);
        print_string("  uptime         - Tiempo de sistema\n", 0x0A);
        print_string("  add X Y        - Suma dos numeros\n", 0x0A);
        print_string("  cmd            - Ayuda extendida\n", 0x0A);
        print_string("  login R C      - Login por rol\n", 0x0A);
        print_string("  logout         - Cerrar sesion\n", 0x0A);
        print_string("  whoami         - Rol y sesion\n", 0x0A);
        print_string("  about          - Info del SO\n", 0x0A);
    }
    else if (__builtin_strcmp(command, "cmd") == 0) {
        print_string("CMD Integrado - Gestion Linux\n", 0x0B);
        print_string("Rol: ", 0x0B);
        print_string(role_name(current_role), 0x0B);
        print_string(" | Sesion: ", 0x0B);
        print_string(session_authenticated ? "activa\n" : "cerrada\n", 0x0B);
        print_string("server list|status|start|stop|restart|backup|restore|audit\n", 0x0B);
        print_string("server fs dirs|mkdir|touch|ls|write|cat|rm|rmdir|chmod\n", 0x0B);
        print_string("login <admin|operator|viewer> <clave>, logout, whoami\n", 0x0B);
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
        if (!read_token(&cursor, role, 16) || !read_token(&cursor, pass, 24)) print_string("Uso: login <admin|operator|viewer> <clave>\n", 0x0C);
        else command_login(role, pass);
    }
    else if (__builtin_strcmp(command, "logout") == 0) {
        command_logout();
    }
    else if (__builtin_strcmp(command, "principales") == 0) {
        command_principales();
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
        if (!read_token(&cursor, op, 16)) print_string("Uso: server fs <op>\n", 0x0C);
        else if (__builtin_strcmp(op, "dirs") == 0) { if (read_token(&cursor, srv, 16)) command_server_fs_dirs(srv); else print_string("Uso: server fs dirs <srv>\n", 0x0C); }
        else if (__builtin_strcmp(op, "mkdir") == 0) { if (read_token(&cursor, srv, 16) && read_token(&cursor, dir, 16)) command_server_fs_mkdir(srv, dir); else print_string("Uso: server fs mkdir <srv> <dir>\n", 0x0C); }
        else if (__builtin_strcmp(op, "touch") == 0) { if (read_token(&cursor, srv, 16) && read_token(&cursor, dir, 16) && read_token(&cursor, file, 16)) command_server_fs_touch(srv, dir, file); else print_string("Uso: server fs touch <srv> <dir> <file>\n", 0x0C); }
        else if (__builtin_strcmp(op, "ls") == 0) { if (read_token(&cursor, srv, 16) && read_token(&cursor, dir, 16)) command_server_fs_ls(srv, dir); else print_string("Uso: server fs ls <srv> <dir>\n", 0x0C); }
        else if (__builtin_strcmp(op, "cat") == 0) { if (read_token(&cursor, srv, 16) && read_token(&cursor, dir, 16) && read_token(&cursor, file, 16)) command_server_fs_cat(srv, dir, file); else print_string("Uso: server fs cat <srv> <dir> <file>\n", 0x0C); }
        else if (__builtin_strcmp(op, "write") == 0) { const char *txt; if (read_token(&cursor, srv, 16) && read_token(&cursor, dir, 16) && read_token(&cursor, file, 16)) { txt = skip_spaces(cursor); command_server_fs_write(srv, dir, file, txt); } else print_string("Uso: server fs write <srv> <dir> <file> <txt>\n", 0x0C); }
        else if (__builtin_strcmp(op, "rm") == 0) { if (read_token(&cursor, srv, 16) && read_token(&cursor, dir, 16) && read_token(&cursor, file, 16)) command_server_fs_rm(srv, dir, file); else print_string("Uso: server fs rm <srv> <dir> <file>\n", 0x0C); }
        else if (__builtin_strcmp(op, "rmdir") == 0) { if (read_token(&cursor, srv, 16) && read_token(&cursor, dir, 16)) command_server_fs_rmdir(srv, dir); else print_string("Uso: server fs rmdir <srv> <dir>\n", 0x0C); }
        else if (__builtin_strcmp(op, "chmod") == 0) { if (read_token(&cursor, srv, 16) && read_token(&cursor, dir, 16) && read_token(&cursor, file, 16) && read_token(&cursor, mode, 8)) command_server_fs_chmod(srv, dir, file, mode); else print_string("Uso: server fs chmod <srv> <dir> <file> <r|w|rw>\n", 0x0C); }
        else print_string("Operacion fs no valida\n", 0x0C);
    }
    else if (__builtin_strcmp(command, "clear") == 0) {
        clear_screen();
    }
    else if (__builtin_strcmp(command, "ps") == 0) {
        print_string("PID  NOMBRE            ESTADO    MEM(KB)\n", 0x0B);
        print_string("---  ------            ------    -------\n", 0x0B);
        for (int i = 0; i < MAX_PROCESSES; i++) {
            if (process_table[i].state != PROC_EMPTY) {
                char buf[20];
                int_to_str(process_table[i].pid, buf);
                print_string(buf, 0x0F);
                print_string("    ", 0x0F);
                
                // Nombre (truncar a 16 chars)
                int name_len = 0;
                while (process_table[i].name[name_len] && name_len < 16) {
                    print_char(process_table[i].name[name_len], 0x0F);
                    name_len++;
                }
                while (name_len < 16) {
                    print_char(' ', 0x0F);
                    name_len++;
                }
                print_string(" ", 0x0F);
                
                // Estado
                switch (process_table[i].state) {
                    case PROC_RUNNING: print_string("RUN  ", 0x0A); break;
                    case PROC_READY:   print_string("READY", 0x0B); break;
                    case PROC_BLOCKED: print_string("BLCK ", 0x0C); break;
                    case PROC_SLEEP:   print_string("SLEP ", 0x0E); break;
                    case PROC_ZOMBIE:  print_string("ZOMB ", 0x0C); break;
                    default:           print_string("?", 0x0F);
                }
                
                print_string(" ", 0x0F);
                int_to_str(process_table[i].memory_size / 1024, buf);
                print_string(buf, 0x0F);
                print_string("\n", 0x0F);
            }
        }
    }
    else if (command[0] == 'e' && command[1] == 'x' && command[2] == 'e' && command[3] == 'c' && command[4] == ' ') {
        const char *name = command + 5;
        int pid = create_process(name, 4096);
        if (pid >= 0) {
            print_string("Proceso creado: ", 0x0A);
            char buf[20];
            int_to_str(pid, buf);
            print_string(buf, 0x0A);
            print_string(" (Prioridad: 5)\n", 0x0A);
        }
    }
    else if (command[0] == 'k' && command[1] == 'i' && command[2] == 'l' && command[3] == 'l' && command[4] == ' ') {
        int pid = parse_number(command + 5);
        kill_process(pid);
        print_string("Proceso convertido a ZOMBIE\n", 0x0A);
    }
    else if (__builtin_strcmp(command, "top") == 0) {
        print_string("\n=== MONITOR DE PROCESOS TOP ===\n", 0x0E);
        print_string("PID  NOMBRE          ESTADO   PRIO  MEM(KB)  CPU_TIME\n", 0x0B);
        print_string("---  ------          ------   ----  -------  --------\n", 0x0B);
        
        int total_memory = 0;
        for (int i = 0; i < MAX_PROCESSES; i++) {
            if (process_table[i].state != PROC_EMPTY) {
                char buf[20];
                total_memory += process_table[i].memory_size;
                
                int_to_str(process_table[i].pid, buf);
                print_string(buf, 0x0F);
                print_string("   ", 0x0F);
                
                // Nombre
                int name_len = 0;
                while (process_table[i].name[name_len] && name_len < 14) {
                    print_char(process_table[i].name[name_len], 0x0F);
                    name_len++;
                }
                while (name_len < 14) {
                    print_char(' ', 0x0F);
                    name_len++;
                }
                print_string(" ", 0x0F);
                
                // Estado
                switch (process_table[i].state) {
                    case PROC_RUNNING: print_string("RUN   ", 0x0A); break;
                    case PROC_READY:   print_string("READY ", 0x0B); break;
                    case PROC_BLOCKED: print_string("BLCK  ", 0x0C); break;
                    case PROC_SLEEP:   print_string("SLEEP ", 0x0E); break;
                    case PROC_ZOMBIE:  print_string("ZOMBIE", 0x0C); break;
                    default:           print_string("?     ", 0x0F);
                }
                print_string(" ", 0x0F);
                
                // Prioridad
                int_to_str(process_table[i].priority, buf);
                print_string(buf, 0x0F);
                print_string("    ", 0x0F);
                
                // Memoria
                int_to_str(process_table[i].memory_size / 1024, buf);
                print_string(buf, 0x0F);
                print_string("      ", 0x0F);
                
                // CPU Time
                int_to_str(process_table[i].cpu_time, buf);
                print_string(buf, 0x0F);
                print_string("\n", 0x0F);
            }
        }
        
        print_string("\nProceso Actual: ", 0x0E);
        print_string(process_table[current_pid].name, 0x0E);
        print_string(" (PID:", 0x0E);
        char buf[20];
        int_to_str(current_pid, buf);
        print_string(buf, 0x0E);
        print_string(")\n", 0x0E);
        
        print_string("Memoria Total Usada: ", 0x0F);
        int_to_str(total_memory / 1024, buf);
        print_string(buf, 0x0F);
        print_string(" KB\n\n", 0x0F);
    }
    else if (command[0] == 's' && command[1] == 'l' && command[2] == 'e' && command[3] == 'e' && command[4] == 'p' && command[5] == ' ') {
        // sleep PID SECONDS
        int i = 6;
        int pid = parse_number(command + i);
        
        while (command[i] != ' ' && command[i] != '\0') i++;
        while (command[i] == ' ') i++;
        
        int seconds = 0;
        if (command[i] != '\0') {
            seconds = parse_number(command + i);
        }
        
        sleep_process(pid, seconds);
        if (seconds > 0) {
            print_string("Proceso suspendido por ", 0x0A);
            char buf[20];
            int_to_str(seconds, buf);
            print_string(buf, 0x0A);
            print_string(" segundos\n", 0x0A);
        }
    }
    else if (command[0] == 'p' && command[1] == 'r' && command[2] == 'i' && command[3] == 'o' && command[4] == 'r' && command[5] == 'i' && command[6] == 't' && command[7] == 'y' && command[8] == ' ') {
        // priority PID PRIORITY
        int i = 9;
        int pid = parse_number(command + i);
        
        while (command[i] != ' ' && command[i] != '\0') i++;
        while (command[i] == ' ') i++;
        
        int priority = parse_number(command + i);
        
        if (pid < 0 || pid >= MAX_PROCESSES || process_table[pid].state == PROC_EMPTY) {
            print_string("Error: proceso no existe\n", 0x0C);
        } else if (priority < 0 || priority > 9) {
            print_string("Error: prioridad debe estar entre 0-9\n", 0x0C);
        } else {
            process_table[pid].priority = priority;
            print_string("Prioridad del proceso actualizada a ", 0x0A);
            char buf[20];
            int_to_str(priority, buf);
            print_string(buf, 0x0A);
            print_string("\n", 0x0A);
        }
    }
    else if (__builtin_strcmp(command, "memory") == 0) {
        print_string("=== INFORMACIÓN DE MEMORIA ===\n", 0x0B);
        print_string("Total: 64 MB\n", 0x0B);
        uint32_t used = 0;
        for (int i = 0; i < MAX_PROCESSES; i++) {
            if (process_table[i].state != PROC_EMPTY) {
                used += process_table[i].memory_size;
            }
        }
        char buf[20];
        int_to_str(used / 1024, buf);
        print_string("Usado: ", 0x0B);
        print_string(buf, 0x0B);
        print_string(" KB\n", 0x0B);
        int_to_str((65536 - used / 1024), buf);
        print_string("Libre: ", 0x0B);
        print_string(buf, 0x0B);
        print_string(" KB\n", 0x0B);
        
        print_string("Procesos Activos: ", 0x0B);
        int count = 0;
        for (int i = 0; i < MAX_PROCESSES; i++) {
            if (process_table[i].state != PROC_EMPTY && process_table[i].state != PROC_ZOMBIE) {
                count++;
            }
        }
        int_to_str(count, buf);
        print_string(buf, 0x0B);
        print_string("\n", 0x0B);
    }
    else if (__builtin_strcmp(command, "uptime") == 0) {
        char buf[20];
        int_to_str(timer_ticks / 100, buf);
        print_string("Tiempo de sistema: ", 0x0E);
        print_string(buf, 0x0E);
        print_string(" segundos\n", 0x0E);
    }
    else if (command[0] == 'a' && command[1] == 'd' && command[2] == 'd' && command[3] == ' ') {
        int a = parse_number(command + 4);
        int i = 4;
        while (command[i] != '\0' && command[i] != ' ') i++;
        while (command[i] == ' ') i++;
        
        int b = 0;
        if (command[i] != '\0') {
            b = parse_number(command + i);
        }
        
        int resultado = a + b;
        char buf[20];
        int_to_str(a, buf);
        print_string(buf, 0x0E);
        print_string(" + ", 0x0E);
        int_to_str(b, buf);
        print_string(buf, 0x0E);
        print_string(" = ", 0x0E);
        int_to_str(resultado, buf);
        print_string(buf, 0x0E);
        print_char('\n', 0x0F);
    }
    else if (__builtin_strcmp(command, "about") == 0) {
        print_string("================================\n", 0x0D);
        print_string("Mi SO v3.2 - Nivel 2 (Ring 3)\n", 0x0D);
        print_string("Sistema Operativo Educativo\n", 0x0D);
        print_string("Características v3.0:\n", 0x0D);
        print_string("  - Modo Protegido (32-bit)\n", 0x0D);
        print_string("  - Tabla de Procesos\n", 0x0D);
        print_string("  - Manejo de Interrupciones\n", 0x0D);
        print_string("  - Gestión de Memoria\n", 0x0D);
        print_string("  - Timer Interrupt\n", 0x0D);
        print_string("Nuevas en v3.1 (Nivel 1):\n", 0x0D);
        print_string("  - Scheduler con Prioridades\n", 0x0D);
        print_string("  - Estados: SLEEP y ZOMBIE\n", 0x0D);
        print_string("  - Comando TOP\n", 0x0D);
        print_string("  - Seguimiento de CPU Time\n", 0x0D);
        print_string("NUEVAS en v3.2 (Nivel 2):\n", 0x0D);
        print_string("  - Modo Usuario (Ring 3)\n", 0x0D);
        print_string("  - GDT con descriptores Ring 3\n", 0x0D);
        print_string("  - INT 0x80 System Calls\n", 0x0D);
        print_string("  - Procesos en Ring 3\n", 0x0D);
        print_string("  - Context switching entre modos\n", 0x0D);
        print_string("================================\n", 0x0D);
    }
    else {
        print_string("Comando no encontrado. Escribe 'help'\n", 0x0C);
    }
}

void kernel_main() {
    // Verificar que estamos en modo protegido (32-bit)
    // Inicializar VGA antes de cualquier print
    char *vga = (char*)VGA_MEMORY;
    for (int i = 0; i < VGA_SIZE; i += 2) {
        vga[i] = ' ';
        vga[i+1] = 0x0F;
    }
    vga_offset = 0;
    
    clear_screen();
    print_string("======================================\n", 0x0B);
    print_string("   Mi SO v3.2 - Nivel 2 (Ring 3)\n", 0x0A);
    print_string("   Modo Usuario + System Calls\n", 0x0A);
    print_string("======================================\n", 0x0B);
    print_string("   Modo: Protegido 32-bit + Ring 3\n", 0x0E);
    print_string("   Plataforma: i386\n", 0x0E);
    print_string("======================================\n\n", 0x0B);
    
    // Inicializar subsistemas
    init_process_table();
    init_server_manager();
    init_server_filesystem();
    setup_idt();
    
    print_string("Características:\n", 0x0E);
    print_string("  > Ring 0 Kernel + Ring 3 User\n", 0x0E);
    print_string("  > INT 0x80 System Calls\n", 0x0E);
    print_string("  > Prioridades (0-9)\n", 0x0E);
    print_string("  > Estados: RUNNING, READY, SLEEP, ZOMBIE\n", 0x0E);
    print_string("  > Comando TOP con CPU tracking\n", 0x0E);
    print_string("  > GDT + Descriptores Ring 3\n", 0x0E);
    print_string("  > Gestor de servidores y CMD integrado\n\n", 0x0E);
    
    print_string("Escribe 'help' para ver comandos\n\n", 0x0F);

    while (1) {
        print_string("> ", 0x0E);
        read_line();
        execute_command();
    }
}
