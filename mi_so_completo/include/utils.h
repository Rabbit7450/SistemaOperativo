#ifndef UTILS_H
#define UTILS_H

void copy_string(char *dst, const char *src, int max_len);
int starts_with(const char *str, const char *prefix);
const char* skip_spaces(const char *str);
int read_token(const char **cursor, char *out, int max_len);
int parse_number(const char *str);
void int_to_str(int num, char *buffer);

// Minimal libc replacements used by the kernel
int strcmp(const char *a, const char *b);
int strlen(const char *s);

#endif
