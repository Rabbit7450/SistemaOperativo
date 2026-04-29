[org 0x7c00]
BITS 16

start:
    ; Configurar segmentos
    mov ax, 0
    mov ds, ax
    mov es, ax
    mov ss, ax
    mov sp, 0x7c00           ; Stack debajo del bootloader
    
    mov [boot_drive], dl
    
    ; Limpiar pantalla y modo video
    mov ax, 0x03
    int 0x10

    ; Mostrar mensaje de carga
    mov si, msg_loading
    call print_string

    ; Cargar kernel desde disco
    mov dl, [boot_drive]
    xor ah, ah
    int 0x13

    mov byte [read_tries], 3
.read_try:
    mov bx, 0x1000           ; Dirección de destino (segmento)
    mov es, bx
    mov bx, 0x0000           ; Offset dentro del segmento
    mov ah, 0x02             ; Función: leer sector
    mov al, 30               ; Número de sectores a leer (kernel grande)
    mov ch, 0                ; Cilindro 0
    mov cl, 2                ; Sector 2 (después del bootloader)
    mov dh, 0                ; Cabeza 0
    mov dl, [boot_drive]     ; Unidad desde BIOS
    int 0x13
    
    ; Verificar si hubo error
    jnc .disk_ok
    dec byte [read_tries]
    jnz .read_try
    jmp disk_error
.disk_ok:

    ; Deshabilitar interrupciones y preparar modo protegido
    cli
    lgdt [gdt_descriptor]
    
    ; Cambiar a Protected Mode
    mov eax, cr0
    or eax, 0x1
    mov cr0, eax
    
    ; Salto lejano a 32-bit (limpiar prefetch queue)
    jmp 0x08:protected_mode

disk_error:
    mov si, msg_error
    call print_string
    jmp $                     ; Bucle infinito

print_string:
    mov ah, 0x0e
.loop:
    lodsb
    cmp al, 0
    je .done
    int 0x10
    jmp .loop
.done:
    ret

msg_loading db 'Bootloader v3.1: Cargando kernel...', 0x0d, 0x0a, 0
msg_error db 'ERROR: No se pudo cargar el kernel', 0x0d, 0x0a, 0
boot_drive db 0
read_tries db 0

; ============================================
; GDT (Global Descriptor Table)
; ============================================
gdt_start:
    ; Descriptor Nulo (requerido)
    dq 0x0

    ; Code Descriptor (Offset 0x08)
    dw 0xffff                ; Límite bajo (bits 0-15)
    dw 0x0000                ; Base baja (bits 0-15)
    db 0x00                  ; Base media (bits 16-23)
    db 10011010b             ; Flags + Type
                             ; 1: Válido
                             ; 0: 
                             ; 0: 
                             ; 1: Code
                             ; 1: Conforming
                             ; 0: No readable
                             ; 1: Accessed
    db 11001111b             ; Flags + Límite alto
                             ; 1: Granularidad (4KB)
                             ; 1: 32-bit
                             ; 0: No usado
                             ; 0: No usado
                             ; 1111: Límite alto
    db 0x00                  ; Base alta (bits 24-31)

    ; Data Descriptor (Offset 0x10)
    dw 0xffff                ; Límite bajo
    dw 0x0000                ; Base baja
    db 0x00                  ; Base media
    db 10010010b             ; Flags + Type (Data, RW)
    db 11001111b             ; Flags + Límite alto
    db 0x00                  ; Base alta

    ; Code Descriptor Ring 3 (Offset 0x18, Selector 0x1B)
    dw 0xffff                ; Límite bajo
    dw 0x0000                ; Base baja
    db 0x00                  ; Base media
    db 11111010b             ; Flags + Type (Ring 3 Code)
                             ; 1: DPL Ring 3 (bits 5-6: 11b)
                             ; 1: Code
                             ; 1: Conforming
                             ; 0: No readable
                             ; 1: Accessed
    db 11001111b             ; Flags + Límite alto
    db 0x00                  ; Base alta

    ; Data Descriptor Ring 3 (Offset 0x20, Selector 0x23)
    dw 0xffff                ; Límite bajo
    dw 0x0000                ; Base baja
    db 0x00                  ; Base media
    db 11110010b             ; Flags + Type (Ring 3 Data, RW)
                             ; 1: DPL Ring 3 (bits 5-6: 11b)
                             ; 0: Data
                             ; 0: Expand-down
                             ; 1: Writable
                             ; 0: No accessed
    db 11001111b             ; Flags + Límite alto
    db 0x00                  ; Base alta

gdt_end:

gdt_descriptor:
    dw gdt_end - gdt_start - 1
    dd gdt_start

; ============================================
; Protected Mode (32-bit)
; ============================================
BITS 32
protected_mode:
    mov ax, 0x10             ; Selector de datos
    mov ds, ax
    mov es, ax
    mov fs, ax
    mov gs, ax
    mov ss, ax
    
    mov esp, 0x90000         ; Stack pointer (plenty of space)
    
    ; Saltar al kernel
    jmp 0x1000               ; Kernel cargado en 0x1000

; ============================================
; Llenar hasta 512 bytes y agregar firma MBR
; ============================================
BITS 16
times 510-($-$$) db 0
dw 0xaa55                    ; Firma MBR (booteable)