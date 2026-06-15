#include <efi.h>
#include <efilib.h>
//#include <stdint.h>
#include "../dosbox/cpu.h"
#include "../uefi/uefi_gfx.h"
#include "../uefi/gfx_console.h"
#include <stdbool.h>
#include "../uefi/uefi_keyboard.h"
#include "../uefi/start.h"
#include <string.h>
#include "../dosbox/dosbox_core.h"
#define INT21_AH09_MAX_LEN 512

extern bool program_exit_flag;   // bandera global
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

static inline uint8_t mem_read_ram(uint32_t addr)
{
    return mem[addr];
}

// ======================================================
// Bandera Global
// ======================================================
void dosbox_run_until_exit(void)
{
    program_exit_flag = false;

    while (!program_exit_flag)
    {
        cpu_step();
    }
}


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
        uint8_t c = mem[addr++];   // ← RAM del COM

        if (c == '$')
            break;

        if (c < 0x20 && c != '\n' && c != '\r')
            continue;

        gfx_putc((char)c);
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
        program_exit_flag = true;
	Print(L"[INT21h] AH=4Ch ejecutado\n");
        //cpu_set_ip(0xFFFF);   // señal de fin
        return;
    }

    Print(L"[INT21] AH=%02x no implementado\n", ah);
}

// ======================================================
//  INT 20h — terminar programa
// ======================================================

static void int20h(void)
{
    //cpu_set_ip(0xFFFF);
    program_exit_flag = true;
}

// ======================================================
// INT60 Listar directorio 
// ======================================================


void int60h(void)
{
    uint8_t ah = (cpu.ax >> 8) & 0xFF;
    switch (ah)
    {
        // ---------------------------------------------------------
        // AH = 00h → Listar directorio vía EFI (tu código original)
        // ---------------------------------------------------------
        case 0x00:
        {
            EFI_STATUS Status = efi_ls(gImageHandle, gSystemTable);

            if (EFI_ERROR(Status)) {
                gfx_printf("[INT60] efi_ls fallo (Status=%r)\n", Status);
            } else {
                gfx_printf("[INT60] Directorio listado correctamente\n");
            }
            break;
        }

        // ---------------------------------------------------------
        // AH = 02h → Cargar HELLO.COM desde el fdd_boot.ima (FAT12)
        // ---------------------------------------------------------
        case 0x02:
    {
    CHAR16 FileName[] = L"HELLO.COM";

    EFI_STATUS Status = efi_load_by_name(FileName);

    if (EFI_ERROR(Status)) {
        cpu.flags |= 1;
        break;
    }
    load_com(gExeBuffer, gExeSize);

    // 1. Guardar estado
    CPUState saved = cpu;

    // 2. Copiar a 0x100h
    memcpy(&mem[0x100], gExeBuffer, gExeSize);


    // 3. Preparar CPU
    cpu.cs = gComSegment;
    cpu.ds = gComSegment;
    cpu.es = gComSegment;
    cpu.ip = 0x100;
    cpu.sp = 0xFFFE;
    cpu.ss = gComSegment;

    // 4. Ejecutar COM
    dosbox_run_until_exit();
    Print(L"[INT60h] HELLO.COM terminó, restaurando COMMAND.COM\n");

    // 5. Restaurar estado
    cpu = saved;

    cpu.flags &= ~1;
    break;
    }


        // ---------------------------------------------------------
        // Otros AH no implementados
        // ---------------------------------------------------------
        default:
            gfx_printf("[INT60] AH=%02Xh no implementado\n", ah);
            break;
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
