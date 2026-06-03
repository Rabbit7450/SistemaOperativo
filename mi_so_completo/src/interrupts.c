#include "common.h"
#include "interrupts.h"
#include "process.h"
#include "vga.h"
#include "utils.h"

// IDT structures
typedef struct {
    uint16_t offset_low;
    uint16_t selector;
    uint8_t zero;
    uint8_t type_attr;
    uint16_t offset_high;
} __attribute__((packed)) IDT_Entry;

typedef struct {
    uint16_t limit;
    uint32_t base;
} __attribute__((packed)) IDT_Descriptor;

static IDT_Entry idt[256];
static IDT_Descriptor idt_descriptor;

static inline uint8_t inb(uint16_t port) {
    uint8_t value;
    __asm__ __volatile__("inb %1, %0" : "=a"(value) : "Nd"(port));
    return value;
}

static inline void outb(uint16_t port, uint8_t value) {
    __asm__ __volatile__("outb %0, %1" : : "a"(value), "Nd"(port));
}

static inline void lidt(IDT_Descriptor *desc) {
    __asm__ __volatile__("lidt %0" : : "m"(*desc));
}

static inline void enable_interrupts() {
    __asm__ __volatile__("sti");
}

static void set_idt_gate(int vector, uint32_t handler, uint8_t type_attr) {
    idt[vector].offset_low = (uint16_t)(handler & 0xFFFF);
    idt[vector].selector = 0x08;
    idt[vector].zero = 0;
    idt[vector].type_attr = type_attr;
    idt[vector].offset_high = (uint16_t)((handler >> 16) & 0xFFFF);
}

static void remap_pic() {
    outb(0x20, 0x11);
    outb(0xA0, 0x11);
    outb(0x21, 0x20);
    outb(0xA1, 0x28);
    outb(0x21, 0x04);
    outb(0xA1, 0x02);
    outb(0x21, 0x01);
    outb(0xA1, 0x01);
    outb(0x21, 0xFC);
    outb(0xA1, 0xFF);
}

#define REG_EDI 0
#define REG_ESI 1
#define REG_EBP 2
#define REG_ESP 3
#define REG_EBX 4
#define REG_EDX 5
#define REG_ECX 6
#define REG_EAX 7

#define KBD_BUFFER_SIZE 128
static volatile char keyboard_buffer[KBD_BUFFER_SIZE];
static volatile int keyboard_head = 0;
static volatile int keyboard_tail = 0;
static volatile int keyboard_pending = 0;

static const char keyboard_map[128] = {
    0,  27, '1', '2', '3', '4', '5', '6', '7', '8', '9', '0', '-', '=',
    '\b', '\t', 'q', 'w', 'e', 'r', 't', 'y', 'u', 'i', 'o', 'p', '[', ']',
    '\r', 0,   'a', 's', 'd', 'f', 'g', 'h', 'j', 'k', 'l', ';', '\'', '`',
    0,   '\\', 'z', 'x', 'c', 'v', 'b', 'n', 'm', ',', '.', '/', 0,   '*',
    0,   ' ', 0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,
    0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,
    0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,
    0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,
    0,   0
};

static void keyboard_push(char c) {
    int next = (keyboard_head + 1) % KBD_BUFFER_SIZE;
    if (next == keyboard_tail) return;
    keyboard_buffer[keyboard_head] = c;
    keyboard_head = next;
    keyboard_pending = 1;
}

static int keyboard_pop(char *out) {
    if (keyboard_tail == keyboard_head) return 0;
    *out = keyboard_buffer[keyboard_tail];
    keyboard_tail = (keyboard_tail + 1) % KBD_BUFFER_SIZE;
    return 1;
}

char get_key() {
    char buffered;
    if (keyboard_pop(&buffered)) {
        return buffered;
    }

    while (1) {
        keyboard_pending = 0;
        __asm__ __volatile__("hlt");
        if (keyboard_pop(&buffered)) return buffered;
    }
}

static int user_ptr_ok(uint32_t addr, uint32_t len) {
    uint32_t start = process_table[current_pid].memory_start;
    uint32_t end = start + process_table[current_pid].memory_size;
    if (addr < start) return 0;
    if (addr >= end) return 0;
    if (len == 0) return 1;
    if (addr + len < addr) return 0;
    if (addr + len > end) return 0;
    return 1;
}

static void exception_handler_code(uint32_t vector) {
    static const char *messages[32] = {
        "Divide Error", "Debug", "NMI", "Breakpoint",
        "Overflow", "BOUND Range", "Invalid Opcode", "Device Not Available",
        "Double Fault", "Coprocessor Segment Overrun", "Invalid TSS", "Segment Not Present",
        "Stack Fault", "General Protection", "Page Fault", "Reserved",
        "x87 Floating-Point", "Alignment Check", "Machine Check", "SIMD Floating-Point",
        "Virtualization", "Control Protection", "Reserved", "Reserved",
        "Reserved", "Reserved", "Reserved", "Reserved",
        "Hypervisor Injection", "VMM Communication", "Security", "Reserved"
    };

    print_string("\n[EXCEPTION] ", 0x0C);
    if (vector < 32) print_string(messages[vector], 0x0C);
    else print_string("Unknown", 0x0C);
    print_string("\nSistema detenido.\n", 0x0C);

    while (1) {
        __asm__ __volatile__("cli; hlt");
    }
}

#define DEFINE_ISR_NOERR(n) \
    __attribute__((naked)) void isr##n##_stub() { \
        __asm__ __volatile__( \
            "pusha\n" \
            "push $" #n "\n" \
            "call exception_handler_code\n" \
            "add $4, %%esp\n" \
                "popa\n" \
                "iret\n" \
            : : : "memory"); \
    }

#define DEFINE_ISR_ERR(n) \
    __attribute__((naked)) void isr##n##_stub() { \
        __asm__ __volatile__( \
            "pusha\n" \
            "push $" #n "\n" \
            "call exception_handler_code\n" \
            "add $4, %%esp\n" \
                "popa\n" \
                "add $4, %%esp\n" \
                "iret\n" \
            : : : "memory"); \
    }

DEFINE_ISR_NOERR(0)
DEFINE_ISR_NOERR(1)
DEFINE_ISR_NOERR(2)
DEFINE_ISR_NOERR(3)
DEFINE_ISR_NOERR(4)
DEFINE_ISR_NOERR(5)
DEFINE_ISR_NOERR(6)
DEFINE_ISR_NOERR(7)
DEFINE_ISR_ERR(8)
DEFINE_ISR_NOERR(9)
DEFINE_ISR_ERR(10)
DEFINE_ISR_ERR(11)
DEFINE_ISR_ERR(12)
DEFINE_ISR_ERR(13)
DEFINE_ISR_ERR(14)
DEFINE_ISR_NOERR(15)
DEFINE_ISR_NOERR(16)
DEFINE_ISR_ERR(17)
DEFINE_ISR_NOERR(18)
DEFINE_ISR_NOERR(19)
DEFINE_ISR_NOERR(20)
DEFINE_ISR_NOERR(21)
DEFINE_ISR_NOERR(22)
DEFINE_ISR_NOERR(23)
DEFINE_ISR_NOERR(24)
DEFINE_ISR_NOERR(25)
DEFINE_ISR_NOERR(26)
DEFINE_ISR_NOERR(27)
DEFINE_ISR_NOERR(28)
DEFINE_ISR_NOERR(29)
DEFINE_ISR_ERR(30)
DEFINE_ISR_NOERR(31)

static void keyboard_irq_handler() {
    uint8_t scancode = inb(0x60);
    if ((scancode & 0x80) == 0 && scancode < 128) {
        char c = keyboard_map[scancode];
        if (c != 0) keyboard_push(c);
    }
    outb(0x20, 0x20);
}

static uint32_t syscall_handler(uint32_t syscall_num, uint32_t arg1, uint32_t arg2) {
    switch (syscall_num) {
        case SYS_EXIT:
            kill_process(current_pid);
            return 0;
        case SYS_GETPID:
            return current_pid;
        case SYS_GETMEM:
            return process_table[current_pid].memory_size;
        case SYS_SLEEP:
            sleep_process(current_pid, arg1);
            return 0;
        case SYS_SETPRIO:
            if (arg1 < MAX_PROCESSES && process_table[arg1].state != PROC_EMPTY && arg2 <= 9) {
                process_table[arg1].priority = arg2;
                return 0;
            }
            return (uint32_t)-1;
        case SYS_GETPRIO:
            if (arg1 < MAX_PROCESSES && process_table[arg1].state != PROC_EMPTY) {
                return process_table[arg1].priority;
            }
            return (uint32_t)-1;
        case SYS_WRITE: {
            const char *p = (const char*)arg1;
            uint32_t max = arg2;
            if (max > 200) max = 200;
            if (!user_ptr_ok((uint32_t)p, max)) return (uint32_t)-1;
            for (uint32_t i = 0; i < max; i++) {
                char c = p[i];
                if (c == '\0') break;
                print_char(c, 0x0F);
            }
            return 0;
        }
        case SYS_READ: {
            // arg1 = buffer pointer, arg2 = max length
            if (!user_ptr_ok(arg1, arg2)) return (uint32_t)-1;
            char *buf = (char *)arg1;
            char c = get_key();
            buf[0] = c;
            print_char(c, 0x0F);
            return 1;
        }
        case SYS_FORK: {
            // Crear proceso hijo como copia del padre
            if (next_pid >= MAX_PROCESSES) return (uint32_t)-1;
            int child_pid = create_process(process_table[current_pid].name, 0x2000);
            if (child_pid < 0) return (uint32_t)-1;
            process_table[child_pid].parent_pid = current_pid;
            process_table[child_pid].context = process_table[current_pid].context;
            return child_pid;
        }
        case SYS_WAIT: {
            // arg1 = child PID a esperar
            int child_pid = arg1;
            if (child_pid < 1 || child_pid >= MAX_PROCESSES) return (uint32_t)-1;
            if (process_table[child_pid].parent_pid != current_pid) return (uint32_t)-1;
            // Esperar a que el hijo termine
            while (process_table[child_pid].state != PROC_ZOMBIE && 
                   process_table[child_pid].state != PROC_EMPTY) {
                sleep_process(current_pid, 1);
            }
            int exit_code = process_table[child_pid].exit_code;
            process_table[child_pid].state = PROC_EMPTY;
            return exit_code;
        }
        case SYS_YIELD: {
            // Ceder voluntariamente el CPU al siguiente proceso
            process_table[current_pid].state = PROC_READY;
            process_tick();
            return 0;
        }
        case SYS_GETPPID: {
            // Obtener PID del proceso padre
            return process_table[current_pid].parent_pid;
        }
        default:
            return (uint32_t)-1;
    }
}

static void syscall_from_regs(uint32_t *regs) {
    if (process_table[current_pid].mode != USER_MODE) {
        regs[REG_EAX] = (uint32_t)-1;
        return;
    }
    regs[REG_EAX] = syscall_handler(regs[REG_EAX], regs[REG_EBX], regs[REG_ECX]);
}

static void timer_irq_dispatch(uint32_t *regs) {
    int previous_pid = current_pid;
    save_context_from_regs(previous_pid, regs);
    process_tick();
    if (current_pid != previous_pid) {
        load_context_to_regs(current_pid, regs);
    }
    outb(0x20, 0x20);
}

__attribute__((naked)) static void irq0_stub() {
    __asm__ __volatile__(
        "pusha\n"
        "mov %esp, %eax\n"
        "push %eax\n"
        "call timer_irq_dispatch\n"
        "add $4, %esp\n"
        "popa\n"
        "iret\n"
    );
}

__attribute__((naked)) static void irq1_stub() {
    __asm__ __volatile__(
        "pusha\n"
        "call keyboard_irq_handler\n"
        "popa\n"
        "iret\n"
    );
}

__attribute__((naked)) static void int80_stub() {
    __asm__ __volatile__(
        "pusha\n"
        "mov %esp, %eax\n"
        "push %eax\n"
        "call syscall_from_regs\n"
        "add $4, %esp\n"
        "popa\n"
        "iret\n"
    );
}

void user_program() {
    __asm__ __volatile__(
        "push $0\n"
        "push $'\\n'\n"
        "push $'!'\n"
        "push $'O'\n"
        "push $'S'\n"
        "push $' '\n"
        "push $'i'\n"
        "push $'M'\n"
        "mov %%esp, %%ebx\n"
        "mov $8, %%ecx\n"
        "mov $7, %%eax\n"
        "int $0x80\n"
        "add $32, %%esp\n"
        "mov $1, %%eax\n"
        "mov $0, %%ebx\n"
        "int $0x80\n"
        :
        :
        : "eax", "ebx", "ecx"
    );
    while (1) {}
}

static void enter_user_mode(uint32_t entry_point, uint32_t stack_top) {
    __asm__ __volatile__(
        "mov %0, %%esp\n"
        "mov $0x23, %%ax\n"
        "mov %%ax, %%ds\n"
        "mov %%ax, %%es\n"
        "mov %%ax, %%fs\n"
        "mov %%ax, %%gs\n"
        "push $0x23\n"
        "push %%esp\n"
        "pushf\n"
        "push $0x1b\n"
        "push %1\n"
        "iret\n"
        :
        : "r"(stack_top), "r"(entry_point)
        : "ax"
    );
}

void run_user_process(int pid) {
    if (pid <= 0 || pid >= MAX_PROCESSES || process_table[pid].state == PROC_EMPTY) {
        print_string("Error: proceso no existe\n", 0x0C);
        return;
    }
    if (process_table[pid].mode != USER_MODE || !process_table[pid].context.valid) {
        print_string("Error: proceso usuario invalido\n", 0x0C);
        return;
    }

    if (process_table[current_pid].state == PROC_RUNNING) {
        process_table[current_pid].state = PROC_READY;
    }
    current_pid = pid;
    process_table[current_pid].state = PROC_RUNNING;
    enter_user_mode(process_table[pid].context.eip, process_table[pid].context.esp);
}

void setup_idt() {
    for (int i = 0; i < 256; i++) {
        idt[i].offset_low = 0;
        idt[i].selector = 0x08;
        idt[i].zero = 0;
        idt[i].type_attr = 0;
        idt[i].offset_high = 0;
    }

    set_idt_gate(0x00, (uint32_t)isr0_stub, 0x8E);
    set_idt_gate(0x01, (uint32_t)isr1_stub, 0x8E);
    set_idt_gate(0x02, (uint32_t)isr2_stub, 0x8E);
    set_idt_gate(0x03, (uint32_t)isr3_stub, 0x8E);
    set_idt_gate(0x04, (uint32_t)isr4_stub, 0x8E);
    set_idt_gate(0x05, (uint32_t)isr5_stub, 0x8E);
    set_idt_gate(0x06, (uint32_t)isr6_stub, 0x8E);
    set_idt_gate(0x07, (uint32_t)isr7_stub, 0x8E);
    set_idt_gate(0x08, (uint32_t)isr8_stub, 0x8E);
    set_idt_gate(0x09, (uint32_t)isr9_stub, 0x8E);
    set_idt_gate(0x0A, (uint32_t)isr10_stub, 0x8E);
    set_idt_gate(0x0B, (uint32_t)isr11_stub, 0x8E);
    set_idt_gate(0x0C, (uint32_t)isr12_stub, 0x8E);
    set_idt_gate(0x0D, (uint32_t)isr13_stub, 0x8E);
    set_idt_gate(0x0E, (uint32_t)isr14_stub, 0x8E);
    set_idt_gate(0x0F, (uint32_t)isr15_stub, 0x8E);
    set_idt_gate(0x10, (uint32_t)isr16_stub, 0x8E);
    set_idt_gate(0x11, (uint32_t)isr17_stub, 0x8E);
    set_idt_gate(0x12, (uint32_t)isr18_stub, 0x8E);
    set_idt_gate(0x13, (uint32_t)isr19_stub, 0x8E);
    set_idt_gate(0x14, (uint32_t)isr20_stub, 0x8E);
    set_idt_gate(0x15, (uint32_t)isr21_stub, 0x8E);
    set_idt_gate(0x16, (uint32_t)isr22_stub, 0x8E);
    set_idt_gate(0x17, (uint32_t)isr23_stub, 0x8E);
    set_idt_gate(0x18, (uint32_t)isr24_stub, 0x8E);
    set_idt_gate(0x19, (uint32_t)isr25_stub, 0x8E);
    set_idt_gate(0x1A, (uint32_t)isr26_stub, 0x8E);
    set_idt_gate(0x1B, (uint32_t)isr27_stub, 0x8E);
    set_idt_gate(0x1C, (uint32_t)isr28_stub, 0x8E);
    set_idt_gate(0x1D, (uint32_t)isr29_stub, 0x8E);
    set_idt_gate(0x1E, (uint32_t)isr30_stub, 0x8E);
    set_idt_gate(0x1F, (uint32_t)isr31_stub, 0x8E);

    remap_pic();
    set_idt_gate(0x20, (uint32_t)irq0_stub, 0x8E);
    set_idt_gate(0x21, (uint32_t)irq1_stub, 0x8E);
    set_idt_gate(0x80, (uint32_t)int80_stub, 0xEE);

    idt_descriptor.limit = (uint16_t)(sizeof(idt) - 1);
    idt_descriptor.base = (uint32_t)&idt;
    lidt(&idt_descriptor);
    enable_interrupts();
}
