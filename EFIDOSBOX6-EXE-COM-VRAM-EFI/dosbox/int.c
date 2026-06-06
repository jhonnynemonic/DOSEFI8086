#include <efi.h>
#include <efilib.h>
//#include <stdint.h>
#include "../dosbox/cpu.h"
#include "../uefi/uefi_gfx.h"
#include "../uefi/gfx_console.h"
#include <stdbool.h>
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


if (ah == 0x09) {

    uint16_t dx = cpu_get_dx();
    uint16_t ds = cpu_get_ds();
    uint16_t es = cpu_get_es();
    uint16_t di = cpu_get_di();

    Print(L"[INT21h] AH=09h ejecutado DS=%04x ES=%04x DX=%04x DI=%04x\n",
          ds, es, dx, di);

    //---------------------------------------------------------
    // Selección del puntero real (tu heurística)
    //---------------------------------------------------------
    uint32_t addr;
    if (dx == 0)
        addr = ((uint32_t)es << 4) + di;
    else
        addr = ((uint32_t)ds << 4) + dx;

    //---------------------------------------------------------
    // 1) DUMP usando una copia del puntero (NO toca addr)
    //---------------------------------------------------------
    Print(L"[INT21h] Dump desde dirección %08x:\n", addr);

    uint32_t dump_ptr = addr;
    int dump_count = 0;
    bool found_dollar = false;

    while (dump_count < INT21_AH09_MAX_LEN) {
        uint8_t c = mem_read8(dump_ptr++);
        dump_count++;

        Print(L"%02x ", c);
        if (dump_count % 16 == 0)
            Print(L"\n");

        if (c == '$') {
            found_dollar = true;
            break;
        }

        // Si hay demasiados ceros seguidos, probablemente puntero inválido
        if (c == 0x00 && dump_count > 32) {
            Print(L"\n[INT21h] Muchos 00h detectados; posible puntero inválido.\n");
            break;
        }
    }

    Print(L"\n[INT21h] Dump finalizado (%d bytes leídos)%s\n",
          dump_count,
          found_dollar ? L" (se encontró '$')" : L" (NO se encontró '$')");

    if (!found_dollar) {
        Print(L"[INT21h] ERROR: No se encontró '$'. No se imprime cadena.\n");
        return;
    }

    //---------------------------------------------------------
    // 2) IMPRESIÓN REAL usando SOLO addr (sin duplicar)
    //---------------------------------------------------------
    Print(L"[INT21h] Imprimiendo cadena:\n");

    uint32_t str_ptr = addr;
    int str_count = 0;

    while (str_count < INT21_AH09_MAX_LEN) {
        uint8_t c = mem_read8(str_ptr++);
        str_count++;

        if (c == '$')
            break;

        // Saltar control chars excepto \n y \r
        if (c < 0x20 && c != '\n' && c != '\r')
            continue;

        // Consola UEFI
        CHAR16 out[2] = { (CHAR16)c, 0 };
        Print(out);

        // Pantalla gráfica
        gfx_putc((char)c);
    }

    Print(L"\n[INT21h] Fin de cadena (longitud %d bytes)\n", str_count);
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
