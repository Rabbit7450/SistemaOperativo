[org 0x7c00]
BITS 16

start:
    mov ax, 0x03
    int 0x10

    mov si, msg_loading
    call print_string

    mov bx, 0x1000
    mov dh, 15               ; Cargamos 15 sectores (para el kernel más grande)
    call disk_load

    jmp 0x1000:0x0000

print_string:
    mov ah, 0x0e
.loop: lodsb
    cmp al, 0
    je .done
    int 0x10
    jmp .loop
.done: ret

disk_load:
    pusha
    mov ah, 0x02
    mov al, dh
    mov ch, 0
    mov dh, 0
    mov cl, 2
    int 0x13
    popa
    ret

msg_loading db 13,10,'Cargando Mi SO v2.0...',13,10,0

times 510-($-$$) db 0
dw 0xaa55