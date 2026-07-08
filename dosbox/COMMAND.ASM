org 100h

start:
    ; Mensaje de bienvenida
    mov dx, welcome
    mov ah, 09h
    int 21h

main_loop:
    ; Mostrar menu
    mov dx, menu
    mov ah, 09h
    int 21h

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

    cmp al, 'o'
    je listar_fat12

    cmp al, '1'
    je abrir_hello

    ; Si no es 'e', volver al prompt
    jmp main_loop

listar_fat12:
    ; Llamar a tu INT personalizada
    mov ah, 00h
    int 60h        ; <-- tu emulador intercepta esto

    jmp main_loop

abrir_hello:
    ; Llamar a tu INT personalizada
    mov ah, 02h
    int 60h        ; <-- tu emulador intercepta esto

    jmp main_loop

salir:
    mov dx, msgsalir
    mov ah, 09h
    int 21h

    int 20h        ; Terminar programa DOS

; -------------------------
; Datos
; -------------------------

welcome   db 'Bienvenido a COMMAND.COM (UEFI-DOS).Presione "e" para salir.Presione "o" para listar.$'
menu	  db 'Presiona: 1.Ejecutar HELLO.COM$'
prompt    db 0Dh,0Ah,'C> $'
msgsalir  db 0Dh,0Ah,'Saliendo de Command$'
