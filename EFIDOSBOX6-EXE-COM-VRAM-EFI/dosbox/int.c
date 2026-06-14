#include <efi.h>
#include <efilib.h>
//#include <stdint.h>
#include "../dosbox/cpu.h"
#include "../uefi/uefi_gfx.h"
#include "../uefi/gfx_console.h"
#include <stdbool.h>
#include "../uefi/uefi_keyboard.h"
#include "../uefi/start.h"
#define INT21_AH09_MAX_LEN 512


extern uint8_t  mem_read8(uint32_t addr);
extern void     mem_write8(uint32_t addr, uint8_t val);

extern uint16_t cpu_get_ax();
extern uint16_t cpu_get_bx();
extern uint16_t cpu_get_cx();
extern uint16_t cpu_get_dx();
extern uint16_t cpu_get_si();
extern uint16_t cpu_get_di();
extern uint16_t cpu_get_cs();
extern uint16_t cpu_get_ds();
extern uint16_t cpu_get_es();
extern uint16_t cpu_get_ss();
extern uint16_t cpu_get_sp();

extern void cpu_set_ax(uint16_t v);
extern void cpu_set_dx(uint16_t v);
extern void cpu_set_ip(uint16_t v);

extern void cpu_halt();   // opcional: poner IP=FFFF

// ======================================================
//  INT 21h — DOS services 
// ======================================================

static void int21h(void)
{
    uint16_t ax = cpu_get_ax();
    uint8_t  ah = ax >> 8;
    //uint8_t  al = ax & 0xFF;

    uint16_t dx = cpu_get_dx();
    uint16_t ds = cpu_get_ds();
    uint16_t es = cpu_get_es();
    uint16_t di = cpu_get_di();

    // -----------------------------------------
    // AH = 09h — imprimir cadena terminada en '$'
    // -----------------------------------------


// AH = 09h — imprimir cadena DOS terminada en '$'
if (ah == 0x09) {

    uint16_t ds = cpu_get_ds();
    uint16_t dx = cpu_get_dx();

    uint32_t addr = ((uint32_t)ds << 4) + dx;

    while (1) {
        uint8_t c = mem_read8(addr++);
        if (c == '$')
            break;

        // Saltar control chars excepto CR/LF
        if (c < 0x20 && c != '\n' && c != '\r')
            continue;

        gfx_putc((char)c);   // ← SOLO ESTO
    }

    return;
    }





    // -----------------------------------------
    // AH = 02h — imprimir carácter en DL
    // -----------------------------------------
    if (ah == 0x02) {
        Print(L"%c", (char)(dx & 0xFF));
        return;
    }


    // -----------------------------------------
    // AH = 01h — Leer carácter con eco
    // -----------------------------------------
    // AH = 01h — Leer carácter con eco desde buffer DOS
    if (ah == 0x01) {
    
    // Esperar hasta que haya algo en el buffer DOS
    uint8_t ch = 0;
    while ((ch = dos_keyboard_buffer_pop()) == 0) {
        // aquí puedes decidir:
        // - o bien llamar a keyboard_poll() para rellenar
        keyboard_poll();
        // - o bien dejar que el emulador avance y se llame desde el bucle principal
    }
    Print(L"POP=%02x\n", ch);
    // Eco en pantalla (como DOS real)
    gfx_putc((char)ch);

    // AL = carácter
    uint16_t ax2 = cpu_get_ax();
    ax = (ax & 0xFF00) | ch;   // poner AL
    cpu_set_ax(ax);

    return;
    }



    // -----------------------------------------
    // AH = 0Ah — leer buffer (dummy)
    // -----------------------------------------
    if (ah == 0x0A) {
        // devolvemos buffer vacío
        uint32_t addr = ((uint32_t)ds << 4) + dx;
        mem_write8(addr + 1, 0); // número de caracteres leídos
        return;
    }

    // -----------------------------------------
    // AH = 4Ch — terminar programa
    // -----------------------------------------
    if (ah == 0x4C) {
	Print(L"[INT21h] AH=4Ch ejecutado\n");
        cpu_set_ip(0xFFFF);   // señal de fin
        return;
    }

    Print(L"[INT21] AH=%02x no implementado\n", ah);
}

// ======================================================
//  INT 20h — terminar programa
// ======================================================

static void int20h(void)
{
    cpu_set_ip(0xFFFF);
}

// ======================================================
// INT60 Listar directorio 
// ======================================================
static void int60h(void)
{
    // Llamada directa a la función EFI que lista el directorio
    EFI_STATUS Status = efi_ls(gImageHandle, gSystemTable);

    if (EFI_ERROR(Status)) {
        gfx_printf("[INT60] efi_ls fallo (Status=%r)\n", Status);
    } else {
        gfx_printf("[INT60] Directorio listado correctamente\n");
    }
}
// ======================================================
//  INT dispatcher
// ======================================================

void handle_int(uint8_t intnum)
{
    switch (intnum) {

    case 0x60:
	int60h();
	break;

    case 0x20:
        int20h();
        break;

    case 0x21:
        int21h();
        break;

    case 0x10: { // INT 10h
        uint8_t ah = (cpu.ax >> 8) & 0xFF;

        switch (ah) {

        // -----------------------------------------
        // AH = 00h — Set Video Mode
        // -----------------------------------------
        case 0x00: {
            uint8_t mode = cpu.ax & 0xFF;

            if (mode == 0x13) {
                video_set_mode_13h();
            } else {
                gfx_printf("[INT10] Modo %02x no implementado\n", mode);
            }
            return;
        }

        // -----------------------------------------
        // AH = 0Eh — Teletype output (BIOS text)
        // -----------------------------------------
        case 0x0E: {
            char ch = cpu.ax & 0xFF;   // AL contiene el carácter
            gfx_putc(ch);             // <-- AHORA SE VE EN TU GOP
            return;
        }

        default:
            gfx_printf("[INT10] Funcion AH=%02x no implementada\n", ah);
            return;
        }
    }

    default:
        gfx_printf("[INT] Interrupcion %02x no implementada\n", intnum);
        cpu_set_ip(0xFFFF);
        break;
    }
}
