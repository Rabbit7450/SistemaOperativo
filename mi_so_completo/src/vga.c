#include "common.h"
#include "vga.h"

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
        vga[i + 1] = 0x0F;
    }
    vga_offset = 0;
}
