org 100h

; ---------------------------------------
; Inicialización simple
; ---------------------------------------
start:
    mov ax, 0
    mov bx, 0
    mov cx, 0

    ; DX = offset del texto (0120h)
    mov dx, mensaje
    mov ah, 09h
    int 21h

    ; Salir a DOS
    mov ah, 00h
    int 20h

; ---------------------------------------
; Relleno para aumentar tamaño del COM
; (tu loader exige tamaño mínimo)
; ---------------------------------------
times 64 db 90h     ; 64 bytes de NOPs

; ---------------------------------------
; Texto a imprimir
; ---------------------------------------
mensaje:
    db "Hola desde UEFI DOSBOX-UEFI! :)", "$"
