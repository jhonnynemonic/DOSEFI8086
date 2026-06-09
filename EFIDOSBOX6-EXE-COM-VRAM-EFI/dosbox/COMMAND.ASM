org 100h

start:
    ; Mensaje de bienvenida
    mov dx, welcome
    mov ah, 09h
    int 21h

main_loop:
    ; Mostrar prompt
    mov dx, prompt
    mov ah, 09h
    int 21h

    ; Leer UNA tecla
    mov ah, 01h
    int 21h        ; AL = tecla

    ; ¿Es 'e' minúscula?
    cmp al, 'e'
    je salir

    ; Si no es 'e', volver al prompt
    jmp main_loop

salir:
    mov dx, msgsalir
    mov ah, 09h
    int 21h

    int 20h        ; Terminar programa DOS

; -------------------------
; Datos
; -------------------------

welcome   db 'Bienvenido a COMMAND.COM (UEFI-DOS)$'
prompt    db 0Dh,0Ah,'C> $'
msgsalir  db 0Dh,0Ah,'Saliendo de Command$'
