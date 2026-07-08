org 100h

start:
    mov dx, msg
    mov ah, 09h
    int 21h

    mov ah, 01h
    int 21h     ; <-- aquí esperamos una tecla

    int 20h     ; terminar COM

msg db "PULSE UNA TECLA$", 0
