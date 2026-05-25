#include <efi.h>
#include <efilib.h>
//#include <stdint.h>
#include "../dosbox/cpu.h"
#include "../uefi/uefi_gfx.h"

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
if (ah == 0x09) {

    Print(L"[INT21h] AH=09h ejecutado DS=%04x ES=%04x DX=%04x DI=%04x\n",
          ds, es, dx, di);
    //Hack de inicio 
    dx = 0x000C;
    uint32_t addr;


    if (dx == 0)
        addr = ((uint32_t)es << 4) + di;
    else
        addr = ((uint32_t)ds << 4) + dx;

	Print(L"[INT21h] Dump en DS:DX (16 bytes):\n");
	for (int i = 0; i < 16; i++) {
    	Print(L"%02x ", mem_read8(addr + i));
	}
	Print(L"\n");

    for (;;) {
        uint8_t c = mem_read8(addr++);
        if (c == '$')
            break;

        CHAR16 out[2];
        out[0] = (CHAR16)c;
        out[1] = 0;
        Print(out);
    }

    Print(L"\n");
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
    // AH = 01h — leer carácter (dummy)
    // -----------------------------------------
    if (ah == 0x01) {
        cpu_set_ax(0);   // AL=0
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
//  INT dispatcher
// ======================================================

void handle_int(uint8_t intnum)
{
    switch (intnum) {

    case 0x20:
        int20h();
        break;

    case 0x21:
        int21h();
        break;

    case 0x10: { // INT 10h
    uint8_t ah = (cpu.ax >> 8) & 0xFF;

    switch (ah) {

    case 0x00: { // Set Video Mode
        uint8_t mode = cpu.ax & 0xFF;

        if (mode == 0x13) {
            video_set_mode_13h();
        } else {
            Print(L"[INT10] Modo %02x no implementado\n", mode);
        }

        return;
    }

    default:
        Print(L"[INT10] Función AH=%02x no implementada\n", ah);
        return;
    }
}

    default:
        Print(L"[INT] Interrupción %02x no implementada\n", intnum);
        cpu_set_ip(0xFFFF);
        break;
    }
}
