#include <efi.h>
#include <efilib.h>
#include <stdint.h>
#include "../dosbox/cpu_486.h"
#include "../uefi/uefi_gfx.h"
#include "../uefi/gfx_console.h"
#include <stdbool.h>
#include "../uefi/uefi_keyboard.h"
#include "../uefi/start.h"
#include <string.h>
#include "../dosbox/dosbox_core.h"
#include "../dosbox/memory.h"

#define INT21_AH09_MAX_LEN 512

// En algún header común (gfx.h) pon al menos los prototipos:
void gfx_putc_at(char ch, int x, int y);
void gfx_scroll_up(int pixels);

// Cursor global simple
static int cur_x = 0;
static int cur_y = 0;

// Parámetros de texto (ajusta a tu resolución real)
#define TEXT_COLS 80
#define TEXT_ROWS 25
#define CHAR_W 8
#define CHAR_H 16

// Ajusta estos a tu resolución real
#define SCREEN_W 640
#define SCREEN_H 480

#define CHARS_PER_LINE (SCREEN_W / CHAR_W)
#define MAX_LINES      (SCREEN_H / CHAR_H)


// ======================================================
// Geometría del disco (floppy o HDD)
// ======================================================

typedef struct {
    bool     is_floppy;            // true = floppy, false = HDD
    uint16_t heads;                // número de cabezas
    uint16_t sectors_per_track;    // sectores por pista
    uint16_t cylinders;            // cilindros
    uint32_t hidden_sectors;       // 0 en floppy, 63 en HDD
} disk_geometry_t;

disk_geometry_t gDiskGeom;


// ======================================================
// Detectar geometría según tamaño del .IMG
// ======================================================

void detect_geometry(uint32_t img_size)
{
    //
    // Caso 1: Floppy's exacto (X bytes) //ULTRAISO PROPERTIES CHECK
    //

    if (img_size == 1474560 )
    {
        gDiskGeom.is_floppy         = true;
        gDiskGeom.heads             = 2;
        gDiskGeom.sectors_per_track = 18;
        gDiskGeom.cylinders         = 80;
        gDiskGeom.hidden_sectors    = 0;
        return;
    }

    //
    // Caso 2: Floppy 720KB (737280 bytes)
    //
    if (img_size == 737280)
    {
        gDiskGeom.is_floppy         = true;
        gDiskGeom.heads             = 2;
        gDiskGeom.sectors_per_track = 9;
        gDiskGeom.cylinders         = 80;
        gDiskGeom.hidden_sectors    = 0;
        return;
    }

    //
    // Caso 3: Floppy 360KB (368640 bytes)
    //
    if (img_size == 368640)
    {
        gDiskGeom.is_floppy         = true;
        gDiskGeom.heads             = 1;
        gDiskGeom.sectors_per_track = 9;
        gDiskGeom.cylinders         = 40;
        gDiskGeom.hidden_sectors    = 0;
        return;
    }

    //
    // Caso 4: Floppy 320KB (327680 bytes)
    //
    if (img_size == 327680)
    {
        gDiskGeom.is_floppy         = true;
        gDiskGeom.heads             = 2;
        gDiskGeom.sectors_per_track = 8;
        gDiskGeom.cylinders         = 40;
        gDiskGeom.hidden_sectors    = 0;
        return;
    }

    //
    // Caso 5: Floppy 180KB (184320 bytes)
    //
    if (img_size == 184320)
    {
        gDiskGeom.is_floppy         = true;
        gDiskGeom.heads             = 1;
        gDiskGeom.sectors_per_track = 8;
        gDiskGeom.cylinders         = 40;
        gDiskGeom.hidden_sectors    = 0;
        return;
    }

    //
    // Caso 6: Floppy 160KB (163840 bytes)
    //
    if (img_size == 163840)
    {
        gDiskGeom.is_floppy         = true;
        gDiskGeom.heads             = 1;
        gDiskGeom.sectors_per_track = 8;
        gDiskGeom.cylinders         = 40;
        gDiskGeom.hidden_sectors    = 0;
        return;
    }

    //
    // Caso 7: HDD (cualquier otro tamaño)
    //
    gDiskGeom.is_floppy         = false;
    gDiskGeom.heads             = 16;
    gDiskGeom.sectors_per_track = 63;
    gDiskGeom.cylinders         = img_size / (512 * 63 * 16);
    gDiskGeom.hidden_sectors    = 63;
}


extern bool     program_exit_flag;   // bandera global
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
extern void cpu_set_bx(uint16_t v);
extern void cpu_set_cx(uint16_t v);
extern void cpu_set_ah(uint8_t v);
extern void cpu_set_al(uint8_t v);

extern void cpu_step(void);
extern void cpu_halt();   // opcional: poner IP=FFFF

extern uint8_t *mem;     // RAM del emulador (definida en dosbox_core)

static inline uint8_t mem_read_ram(uint32_t addr)
{
    return mem[addr];
}

static inline void cpu_clear_cf(void)
{
    cpu.eflags &= ~FLAG_CF;
}


// ======================================================
// Bucle principal: correr hasta que el programa DOS salga
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
    uint16_t dx = cpu_get_dx();
    uint16_t ds = cpu_get_ds();
    uint16_t es = cpu_get_es();
    uint16_t di = cpu_get_di();

    // -----------------------------------------
    // AH = 09h — imprimir cadena terminada en '$'
    // -----------------------------------------
    if (ah == 0x09) {

        uint16_t ds = cpu_get_ds();
        uint16_t dx = cpu_get_dx();

        uint32_t addr = ((uint32_t)ds << 4) + dx;

        while (1) {
            uint8_t c = mem[addr++];   // RAM del COM

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
    if (ah == 0x01) {

        uint8_t ch = 0;
        while ((ch = dos_keyboard_buffer_pop()) == 0) {
            keyboard_poll();
        }
        Print(L"POP=%02x\n", ch);

        gfx_putc((char)ch);

        uint16_t ax2 = cpu_get_ax();
        ax2 = (ax2 & 0xFF00) | ch;   // poner AL
        cpu_set_ax(ax2);

        return;
    }

    // -----------------------------------------
    // AH = 0Ah — leer buffer (dummy)
    // -----------------------------------------
    if (ah == 0x0A) {
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
        return;
    }

    Print(L"[INT21] AH=%02x no implementado\n", ah);
}

// ======================================================
//  INT 20h — terminar programa
// ======================================================

static void int20h(void)
{
    program_exit_flag = true;
}

// ======================================================
// INT60 — servicios propios
// ======================================================

void int60h(void)
{
    uint8_t ah = (cpu_get_ax() >> 8) & 0xFF;

    switch (ah)
    {
        // ---------------------------------------------------------
        // AH = 00h → Listar directorio vía EFI
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
                cpu.eflags |= 1;   // CF=1
                break;
            }

            load_com(gExeBuffer, gExeSize);

            // 1. Guardar estado completo del 486
            CPUState saved = cpu;

            // 2. Copiar a 0x100h
            memcpy(&mem[0x100], gExeBuffer, gExeSize);

            // 3. Preparar CPU en modo real 486
            cpu.cs  = gComSegment;
            cpu.ds  = gComSegment;
            cpu.es  = gComSegment;
            cpu.ss  = gComSegment;
            cpu.esp = 0xFFFE;
            cpu_set_ip(0x0100);

            // 4. Ejecutar COM
            dosbox_run_until_exit();
            Print(L"[INT60h] HELLO.COM terminó, restaurando COMMAND.COM\n");

            // 5. Restaurar estado
            cpu = saved;

            cpu.eflags &= ~1u;  // CF=0
            break;
        }

        default:
            gfx_printf("[INT60] AH=%02Xh no implementado\n", ah);
            break;
    }
}
// ======================================================
// INT 13h - Fat Disk
// ======================================================
uint8_t int13_read_sector_lba(uint32_t lba, uint8_t *dst)
{
    if (!gExeBufferDisk)
        return 1;

    uint32_t offset = lba * 512;

    if (offset + 512 > gExeSizeDisk)
        return 1;   // fuera de rango
    
    memcpy(dst, gExeBufferDisk + offset, 512);
    Print(L"[INT13 READ LBA] lba=%d offset=%d size=%d CF=%d\n",
      lba, offset, gExeSizeDisk, (cpu.eflags & FLAG_CF) ? 1 : 0);

    return 0;       // OK
}

uint8_t int13_write_sector_lba(uint32_t lba, const uint8_t *src)
{
    if (!gExeBufferDisk)
        return 1;

    uint32_t offset = lba * 512;

    if (offset + 512 > gExeSizeDisk)
        return 1;   // fuera de rango

    memcpy(gExeBufferDisk + offset, src, 512);
    return 0;       // OK
}


void int13h(void)
{
Print(L"[INT13 GEOM] is_floppy=%d HEADS=%d SPT=%d CYL=%d\n",
      gDiskGeom.is_floppy, gDiskGeom.heads,
      gDiskGeom.sectors_per_track, gDiskGeom.cylinders);
 

   uint8_t ah = get_ah();

    switch (ah)
    {
        // ---------------------------------------------------------
        // AH = 00h — RESET DISK SYSTEM
        // ---------------------------------------------------------
        case 0x00:
        {
            set_ah(0);
            cpu.eflags &= ~FLAG_CF;
            return;
        }

        // ---------------------------------------------------------
        // AH = 01h — GET STATUS
        // ---------------------------------------------------------
        case 0x01:
        {
            set_ah(0);
            cpu.eflags &= ~FLAG_CF;
            return;
        }

        // ---------------------------------------------------------
        // AH = 02h — READ SECTORS (CHS)
        // ---------------------------------------------------------
case 0x02:
{
    uint8_t  sectors_to_read = get_al();
    uint16_t es = cpu.es;
    uint16_t bx = get_bx();

    uint16_t cx = get_cx();
    uint16_t dx = get_dx();

    uint8_t  sector   = cx & 0x3F;   // CL bits 0..5
    uint8_t  head     = dx >> 8;     // DH
    uint16_t cylinder = cx >> 8;     // CH (floppy: nada de CL bits 6..7)

    uint16_t SPT   = gDiskGeom.sectors_per_track; // 18
    uint16_t HEADS = gDiskGeom.heads;             // 2

    uint32_t lba = (cylinder * HEADS + head) * SPT + (sector - 1);

    uint8_t *dst = mem + (es * 16 + bx);

    for (int i = 0; i < sectors_to_read; i++) {
        if (int13_read_sector_lba(lba + i, dst + i * 512)) {
            set_ah(1);
            cpu.eflags |= FLAG_CF;
            return;
        }
    }

    Print(L"[INT13 READ CHS] C=%d H=%d S=%d  -> LBA=%d  ES:BX=%04X:%04X\n",
          cylinder, head, sector, lba, es, bx);

    set_ah(0);
    cpu.eflags &= ~FLAG_CF;
    return;
}


        // ---------------------------------------------------------
        // AH = 03h — WRITE SECTORS (CHS)
        // ---------------------------------------------------------
        case 0x03:
        {
            uint8_t  sectors_to_write = get_al();
            uint16_t es = cpu.es;
            uint16_t bx = get_bx();

            uint16_t cx = get_cx();
            uint16_t dx = get_dx();

            uint8_t  sector   = cx & 0x3F;
            uint16_t cylinder = (cx >> 8);
            uint8_t  head     = dx >> 8;

            uint16_t SPT   = gDiskGeom.sectors_per_track;
            uint16_t HEADS = gDiskGeom.heads;

            uint32_t lba = (cylinder * HEADS + head) * SPT + (sector - 1);

            uint8_t *src = mem + (es * 16 + bx);

            for (int i = 0; i < sectors_to_write; i++)
            {
                if (int13_write_sector_lba(lba + i, src + i * 512))
                {
                    set_ah(1);
                    cpu.eflags |= FLAG_CF;
                    return;
                }
            }

            set_ah(0);
            cpu.eflags &= ~FLAG_CF;
            return;
        }

        // ---------------------------------------------------------
        // AH = 08h — GET DRIVE PARAMETERS
        // ---------------------------------------------------------
        case 0x08:
        {
            set_ah(0);

            uint16_t cyl   = gDiskGeom.cylinders - 1;
            uint8_t  spt   = gDiskGeom.sectors_per_track;
            uint8_t  heads = gDiskGeom.heads - 1;

            // CH = cylinders, CL = sectors per track
            set_cx((cyl << 8) | spt);

            // DH = heads-1, DL = drive (00h floppy, 80h HDD)
            set_dx((heads << 8) | (gDiskGeom.is_floppy ? 0x00 : 0x80));

            // BL = media type (00h floppy, F8h HDD)
            if (gDiskGeom.is_floppy)
                set_bx(0x0000);   // BL=00h
            else
                set_bx(0xF800);   // BL=F8h

            // AL = drive type (00h floppy, 02h HDD)
            if (gDiskGeom.is_floppy)
                cpu_set_al(0x00);
            else
                cpu_set_al(0x02);

            cpu.eflags &= ~FLAG_CF;
            return;
        }

        // ---------------------------------------------------------
        // AH = 15h — GET DISK TYPE
        // ---------------------------------------------------------
        case 0x15:
        {
            if (gDiskGeom.is_floppy)
                set_ax(0x0002);   // floppy
            else
                set_ax(0x0003);   // hard disk

            set_bx(0x0000);
            set_cx(0x0000);
            cpu.eflags &= ~FLAG_CF;
            return;
        }

        // ---------------------------------------------------------
        // AH = 41h — EXTENSIONS INSTALLED?
        // ---------------------------------------------------------
        case 0x41:
        {
            cpu_set_ah(0x00);        // OK
            cpu_set_al(0x01);        // extensions present
            cpu_set_bx(0xAA55);      // signature
            cpu_set_cx(0x0001);      // version 1.x
            cpu_set_dx(0x0000);      // drive 0
            cpu.eflags &= ~FLAG_CF;
            return;
        }

        // ---------------------------------------------------------
        // AH = 42h — EXTENDED LBA READ (solo HDD)
        // ---------------------------------------------------------
        case 0x42:
        {
            if (gDiskGeom.is_floppy)
            {
                set_ax(0x0001);      // error: no LBA en floppy
                cpu.eflags |= FLAG_CF;
                return;
            }

            uint32_t addr = cpu.es * 16 + get_bx();

            uint16_t count = *(uint16_t *)(mem + addr + 2);
            uint64_t lba   = *(uint64_t *)(mem + addr + 4);
            uint32_t buf   = *(uint32_t *)(mem + addr + 12);

            for (uint16_t i = 0; i < count; i++)
            {
                if (int13_read_sector_lba(lba + i, mem + buf + i * 512))
                {
                    set_ax(0x0001);
                    cpu.eflags |= FLAG_CF;
                    return;
                }
            }

            set_ax(0x0000);
            cpu.eflags &= ~FLAG_CF;
            return;
        }

        // ---------------------------------------------------------
        // DEFAULT — NO IMPLEMENTADO
        // ---------------------------------------------------------
        default:
        {
            gfx_printf("[INT13] AH=%02x no implementado\n", ah);
            set_ah(1);
            cpu.eflags |= FLAG_CF;
            return;
        }
    }
}
// ======================================================
// INT 10
// ======================================================
void bios_do_iret(void)
{
    uint16_t sp = cpu_get_sp();

    uint16_t ip = mem_read16((cpu.ss << 4) + sp);
    sp += 2;

    uint16_t cs = mem_read16((cpu.ss << 4) + sp);
    sp += 2;

    uint16_t flags = mem_read16((cpu.ss << 4) + sp);
    sp += 2;

    cpu_set_sp(sp);

    cpu.cs = cs;
    cpu.eip = ip;
    cpu.eflags = (cpu.eflags & 0xFFFF0000) | flags;
}

// ======================================================
//  INT dispatcher
// ======================================================

void handle_int(uint8_t intnum)
{
    switch (intnum) {

case 0x00: { // INT 0 — Divide Error
    // Aquí NO volvemos a hacer PUSHF/CS/IP,
    // eso ya lo hizo el opcode CD o cpu_interrupt(0).

    gfx_printf("[INT0] Divide error AX=%04X\n", get_ax());

    // Simular IRET BIOS: restaurar IP, CS y FLAGS desde la pila
    bios_do_iret();
    return;
}

    case 0x60:
        int60h();
        break;

    case 0x20:
        int20h();
        break;

    case 0x13:
        int13h();
        break;
    case 0x21:
        int21h();
        break;

case 0x10: { // INT 10h
        uint8_t ah = (cpu_get_ax() >> 8) & 0xFF;

        switch (ah) {

        // -----------------------------------------
        // AH = 00h — Set Video Mode
        // -----------------------------------------
        case 0x00: {
            uint8_t mode = cpu_get_ax() & 0xFF;

            if (mode == 0x13) {
                video_set_mode_13h();
            } else {
                gfx_printf("[INT10] Modo %02x no implementado\n", mode);
            }
            bios_do_iret();
            return;
        }

        // -----------------------------------------
        // AH = 0Eh — Teletype output (BIOS text)
        // -----------------------------------------
        case 0x0E: {
            char ch = cpu_get_ax() & 0xFF;   // AL
            gfx_putc(ch);
            bios_do_iret();
            return;
        }

        default:
            gfx_printf("[INT10] Funcion AH=%02x no implementada\n", ah);
            bios_do_iret();
            return;
        }
    }


    default:
        gfx_printf("[INT] Interrupcion %02x no implementada\n", intnum);
        cpu_set_ip(0xFFFF);
        break;
    }
}
