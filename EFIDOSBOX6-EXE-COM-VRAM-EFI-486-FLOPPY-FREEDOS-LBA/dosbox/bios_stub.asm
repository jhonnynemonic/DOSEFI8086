org 0x0500

; -----------------------------
; Stub BIOS para INT 13h
; -----------------------------
int13_stub:
    ; Guardar registros en zona de comunicación
    mov [0x0100], ax
    mov [0x0102], bx
    mov [0x0104], cx
    mov [0x0106], dx
    mov [0x0108], si
    mov [0x010A], di
    mov [0x010C], es
    mov [0x010E], ds

    ; Señal al host: INT 13h
    mov byte [0x00F0], 0x13

    ; Volver al punto donde se llamó INT
    iret
