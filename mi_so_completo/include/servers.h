#ifndef SERVERS_H
#define SERVERS_H

#include "common.h"

extern ServerService server_table[MAX_SERVERS];
extern int server_count;

extern int current_role;
extern int session_authenticated;

void init_server_manager();
void init_server_filesystem();
void server_tick(int ticks);

const char* role_name(int role);
int require_role(int min_role);

void command_server_list();
void command_server_status(const char *name);
void command_server_start(const char *name);
void command_server_stop(const char *name);
void command_server_restart(const char *name);
void command_server_backup(const char *name);
void command_server_restore(const char *name);
void command_server_audit();
void command_login(const char *role, const char *pass);
void command_logout();
void command_server_fs_dirs(const char *srv);
void command_server_fs_mkdir(const char *srv, const char *dir);
void command_server_fs_touch(const char *srv, const char *dir, const char *file);
void command_server_fs_ls(const char *srv, const char *dir);
void command_server_fs_write(const char *srv, const char *dir, const char *file, const char *content);
void command_server_fs_cat(const char *srv, const char *dir, const char *file);
void command_server_fs_rm(const char *srv, const char *dir, const char *file);
void command_server_fs_rmdir(const char *srv, const char *dir);
void command_server_fs_chmod(const char *srv, const char *dir, const char *file, const char *mode);
void command_principales();
// Apache simulation commands
void command_apache_logs();
void command_apache_simulate(int requests);
void command_apache_ls();
void command_apache_cat(const char *path);
void command_apache_tail(int n);
void command_apache_add(const char *name, const char *content);
void command_apache_rm(const char *name);
void command_apache_vhosts();

#endif
