#include "utils.h"

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
        if (str[i] != prefix[i]) {
            return 0;
        }
        i++;
    }
    return 1;
}

const char* skip_spaces(const char *str) {
    while (*str == ' ') {
        str++;
    }
    return str;
}

int read_token(const char **cursor, char *out, int max_len) {
    const char *p = skip_spaces(*cursor);
    int i = 0;

    if (*p == '\0') {
        return 0;
    }

    while (*p && *p != ' ' && i < max_len - 1) {
        out[i++] = *p++;
    }
    out[i] = '\0';
    *cursor = p;
    return 1;
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
