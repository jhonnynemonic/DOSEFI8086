#include <efi.h>
#include <efilib.h>
//#include "../dosbox/stdint.h"
#include <stdint.h>
#include <stdbool.h>
#include "../dosbox/memory.h"
#include "../dosbox/int.h"
#include "../dosbox/cpu.h"
#include "../uefi/uefi_gfx.h"
#include "../uefi/gfx_console.h"
#include "../uefi/uefi_keyboard.h"
#include "../uefi/start.h"

extern void cpu_reset(void);
extern void cpu_step(void);
extern void cpu_set_cs_ip(uint16_t cs, uint16_t ip);
extern void cpu_set_ss_sp(uint16_t ss, uint16_t sp);

extern uint16_t cpu_get_cs(void);
extern uint16_t cpu_get_ip(void);
extern uint16_t cpu_get_ds(void);
extern uint16_t cpu_get_es(void);
extern uint16_t cpu_get_ss(void);
extern uint16_t cpu_get_sp(void);
extern uint16_t cpu_get_ax(void);
extern uint16_t cpu_get_dx(void);
extern uint16_t cpu_get_di(void);

//extern const uint8_t noname_exe[];
//extern const UINTN   noname_exe_len;
//extern const uint8_t noname_exe_2[];
//extern const UINTN   noname_exe_2_len;
//extern const uint8_t noname_exe3[];
//extern const UINTN   noname_exe3_len;
extern uint8_t  noname_exe_1[];
extern uint32_t noname_exe_1_len;
extern uint8_t  noname_exe_2[];
extern uint32_t noname_exe_2_len;
extern uint8_t  noname_exe_3[];
extern uint32_t noname_exe_3_len;
extern void *gExeBuffer;
extern UINTN gExeSize;

uint16_t gComSegment = 0;

uint8_t *mem = NULL;

// Buffer circular BIOS de 16 bytes
uint8_t dos_keybuf[16];
int dos_key_head = 0;
int dos_key_tail = 0;



void dosbox_init(void)
{
    Print(L"[INIT] Llamando a cpu_reset()\n");
    cpu_reset();

    // Reservar 1 MB si aún no existe
    if (!mem) {
        mem = AllocatePool(1024 * 1024);
        if (!mem) {
            Print(L"[INIT] ERROR: no se pudo reservar memoria\n");
            return;
        }
    }

    // Inicializar la memoria a cero
    for (UINTN i = 0; i < 1024 * 1024; i++)
        mem[i] = 0;

    Print(L"[INIT] Memoria inicializada (1 MB)\n");
    Print(L"[INIT] Saliendo de dosbox_init()\n");
}
// ============================================================
//  CARGA DE ARCHIVOS COM (DOS real)
// ============================================================

void load_com(const uint8_t *program3, UINTN size3)
{
    Print(L"[DOS] Cargando COM...\n");

    if (size3 < 1) {
        Print(L"[DOS] ERROR: COM vacío\n");
        return;
    }

    // Segmento donde cargamos el COM
    uint16_t load_seg  = 0x1000;
    uint32_t load_base = load_seg << 4;

    // Dirección donde empieza el código del COM (ORG 100h)
    uint32_t com_base  = load_base + 0x0100;

    // Tamaño máximo permitido (64K - PSP)
    UINT32 max_com    = 0x10000 - 0x0100;
    UINT32 image_size = (size3 > max_com) ? max_com : (UINT32)size3;

    gComSegment = load_seg;
    Print(L"[DOS] COM: load_seg=%04x load_base=%08x com_base=%08x size=%u\n",
          load_seg, load_base, com_base, image_size);

    // ============================================================
    // 1. CREAR PSP REAL (256 bytes)
    // ============================================================
    for (int i = 0; i < 256; i++)
        mem_write8(load_base + i, 0);

    // Command tail vacío (como DOS real)
    mem_write8(load_base + 0x80, 0);     // longitud
    mem_write8(load_base + 0x81, 0x0D);  // CR
    mem_write8(load_base + 0x82, '$');   // terminador

    // ============================================================
    // 2. COPIAR EL COM EN 0100h
    // ============================================================
    for (UINT32 i = 0; i < image_size; i++)
        mem_write8(com_base + i, program3[i]);

    // ============================================================
    // 3. INICIALIZAR REGISTROS COMO DOS REAL
    // ============================================================
    uint16_t cs = load_seg;
    uint16_t ip = 0x0100;
    uint16_t ss = load_seg;
    uint16_t sp = 0xFFFE;

    cpu_set_cs_ip(cs, ip);
    cpu_set_ss_sp(ss, sp);
    cpu_set_ds(cs);
    cpu_set_es(cs);

    Print(L"[DOS] COM cargado: CS=%04x IP=%04x SS=%04x SP=%04x\n",
          cs, ip, ss, sp);

    Print(L"[DOS] Bytes en entrypoint COM:\n");
    for (int i = 0; i < 16 && i < (int)image_size; i++)
        Print(L"%02x ", mem_read8((cs << 4) + ip + i));
    Print(L"\n");

    Print(L"[DOS] load_com: FIN\n");
}

void load_exe3(const uint8_t *program3, UINTN size3)
{
    typedef struct {
        uint16_t e_magic;
        uint16_t e_cblp;
        uint16_t e_cp;
        uint16_t e_crlc;
        uint16_t e_cparhdr;
        uint16_t e_minalloc;
        uint16_t e_maxalloc;
        uint16_t e_ss;
        uint16_t e_sp;
        uint16_t e_csum;
        uint16_t e_ip;
        uint16_t e_cs;
        uint16_t e_lfarlc;
        uint16_t e_ovno;
        uint16_t e_res[4];
        uint16_t e_oemid;
        uint16_t e_oeminfo;
        uint16_t e_res2[10];
        uint32_t e_lfanew;
    } MZ_HEADER;

    Print(L"[DOS] load_exe3: inicio\n");

    bool is_exe = false;
    const MZ_HEADER *hdr = NULL;

    if (size3 >= sizeof(MZ_HEADER)) {
        hdr = (const MZ_HEADER *)program3;
        uint16_t magic = hdr->e_magic;
        uint16_t magic_be = (magic >> 8) | (magic << 8);

        if (magic_be == 0x4D5A) {
            is_exe = true;
            Print(L"[DOS] firma MZ correcta (big-endian)\n");
        }
    }

    if (is_exe) {
        // ======== CAMINO EXE (tu código tal cual) ========
        UINT32 file_size;
        if (hdr->e_cblp == 0)
            file_size = (UINT32)hdr->e_cp * 512;
        else
            file_size = (UINT32)(hdr->e_cp - 1) * 512 + hdr->e_cblp;

        Print(L"[DOS] file_size = %u\n", file_size);

        UINT32 header_bytes = (UINT32)hdr->e_cparhdr * 16;
        Print(L"[DOS] header_paragraphs=%u header_bytes=%u\n",
              hdr->e_cparhdr, header_bytes);

        INT32 image_size = (INT32)size3 - (INT32)header_bytes;

        if (image_size < 0) {
            INT32 tiny_size = (INT32)size3 - (INT32)header_bytes;
            if (tiny_size < 0)
                tiny_size = 0;

            Print(L"[DOS] EXE tiny detectado, ajustando image_size a %d\n", tiny_size);
            image_size = tiny_size;
        }

        UINT32 max_image = 0;
        if (size3 > header_bytes)
            max_image = (UINT32)(size3 - header_bytes);

        if ((UINT32)image_size > max_image) {
            Print(L"[DOS] Ajustando image_size a max_image=%u\n", max_image);
            image_size = (INT32)max_image;
        }

        uint16_t load_seg  = 0x1000;
        uint32_t load_base = load_seg << 4;

        Print(L"[DOS] load_seg=%04x load_base=%08x\n", load_seg, load_base);

        for (UINT32 i = 0; i < (UINT32)image_size; i++)
            mem_write8(load_base + i, 0);

        for (UINT32 i = 0; i < (UINT32)image_size; i++)
            mem_write8(load_base + i, program3[header_bytes + i]);

        uint16_t rel_count = hdr->e_crlc;
        uint16_t rel_off   = hdr->e_lfarlc;

        Print(L"[DOS] relocaciones (count=%u, off=%u)\n", rel_count, rel_off);

        const uint8_t *rel_ptr = program3 + rel_off;

        for (uint16_t i = 0; i < rel_count; i++) {
            uint16_t off = *(uint16_t *)(rel_ptr + i*4 + 0);
            uint16_t seg = *(uint16_t *)(rel_ptr + i*4 + 2);

            uint32_t addr = (load_seg + seg) * 16 + off;
            uint16_t val  = mem_read8(addr) | (mem_read8(addr+1) << 8);

            val += load_seg;

            mem_write8(addr,     val & 0xFF);
            mem_write8(addr + 1, val >> 8);
        }

        uint16_t cs = load_seg + hdr->e_cs;
        uint16_t ip = hdr->e_ip;
        uint16_t ss = load_seg + hdr->e_ss;

        uint16_t sp = hdr->e_sp;
        if (sp == 0)
            sp = 0x0080;

        cpu_set_cs_ip(cs, ip);
        cpu_set_ss_sp(ss, sp);
        cpu_set_ds(cs);
        cpu_set_es(cs);

        Print(L"[DOS] EXE cargado: CS=%04x IP=%04x SS=%04x SP=%04x\n",
              cs, ip, ss, sp);

        Print(L"[DEBUG] Antes del bucle: CS:IP=%04x:%04x\n",
              cpu_get_cs(), cpu_get_ip());

        Print(L"[DOS] Paso 11: bytes en entrypoint\n");
        for (int i = 0; i < 16; i++)
            Print(L"%02x ", mem_read8((cs << 4) + ip + i));
        Print(L"\n");

        Print(L"[DOS] load_exe3: fin (EXE)\n");
        return;
    }

    // ======== CAMINO COM (lo que me pedías) ========

    Print(L"[DOS] No es EXE MZ válido, tratando como COM\n");

    // Puedes poner un mínimo si quieres, pero ya no es obligatorio:
    if (size3 < 1) {
        Print(L"[DOS] ERROR: COM vacío\n");
        return;
    }

    // Segmento donde cargar el COM
    uint16_t load_seg  = 0x1000;
    uint32_t load_base = load_seg << 4;

    // En COM, el código empieza en offset 0100h dentro del segmento
    uint32_t com_base = load_base + 0x0100;

    // Limitar tamaño para no pisar más de 64K
    UINT32 max_com = 0x10000 - 0x0100; // 64K - 256 bytes
    UINT32 image_size = (size3 > max_com) ? max_com : (UINT32)size3;

    Print(L"[DOS] COM: load_seg=%04x load_base=%08x com_base=%08x size=%u\n",
          load_seg, load_base, com_base, image_size);

    // Opcional: limpiar algo de memoria
    for (UINT32 i = 0; i < image_size; i++)
        mem_write8(com_base + i, 0);

    // Copiar el COM tal cual a partir de 0100h
    for (UINT32 i = 0; i < image_size; i++)
        mem_write8(com_base + i, program3[i]);

    // En un COM real, PSP estaría en load_base, pero tú puedes ignorarlo
    // y simplemente arrancar en CS:0100h

    uint16_t cs = load_seg;
    uint16_t ip = 0x0100;
    uint16_t ss = load_seg;
    uint16_t sp = 0xFFFE;   // o 0xFF00, como prefieras

    cpu_set_cs_ip(cs, ip);
    cpu_set_ss_sp(ss, sp);
    cpu_set_ds(cs);
    cpu_set_es(cs);

    Print(L"[DOS] COM cargado: CS=%04x IP=%04x SS=%04x SP=%04x\n",
          cs, ip, ss, sp);

    Print(L"[DOS] Bytes en entrypoint COM:\n");
    for (int i = 0; i < 16 && i < (int)image_size; i++)
        Print(L"%02x ", mem_read8((cs << 4) + ip + i));
    Print(L"\n");

    Print(L"[DOS] load_exe3: fin (COM)\n");
}

void load_exe1(const uint8_t *program, UINTN size)
{
typedef struct {
    uint16_t e_magic;      // "MZ"
    uint16_t e_cblp;       // Bytes en la última página
    uint16_t e_cp;         // Páginas totales
    uint16_t e_crlc;       // Número de relocations
    uint16_t e_cparhdr;    // Tamaño del header en párrafos
    uint16_t e_minalloc;   // Memoria mínima
    uint16_t e_maxalloc;   // Memoria máxima
    uint16_t e_ss;         // SS inicial (relativo)
    uint16_t e_sp;         // SP inicial
    uint16_t e_csum;       // Checksum
    uint16_t e_ip;         // IP inicial
    uint16_t e_cs;         // CS inicial (relativo)
    uint16_t e_lfarlc;     // Offset tabla relocations
    uint16_t e_ovno;       // Overlay
    uint16_t e_res[4];     // Reservado
    uint16_t e_oemid;      // OEM ID
    uint16_t e_oeminfo;    // OEM info
    uint16_t e_res2[10];   // Reservado
    uint32_t e_lfanew;     // Offset a PE (si existe)
} MZ_HEADER;



    Print(L"[DOS] load_exe1: inicio\n");

    if (size < sizeof(MZ_HEADER)) {
        Print(L"[DOS] ERROR: archivo demasiado pequeño\n");
        return;
    }

    const MZ_HEADER *hdr = (const MZ_HEADER *)program;
    // Little-Endian
    //if (hdr->e_magic != 0x5A4D) {
    //    Print(L"[DOS] ERROR: firma MZ incorrecta\n");
    //    return;
    //}
    // Big-Endian
    uint16_t magic = hdr->e_magic;

    // convertir de little-endian a big-endian
    uint16_t magic_be = (magic >> 8) | (magic << 8);

    if (magic_be != 0x4D5A) {
    Print(L"[DOS] ERROR: firma MZ incorrecta (big-endian)\n");
    return;
    }
    

Print(L"[DOS] firma MZ correcta (big-endian)\n");

    UINT32 file_size;
    if (hdr->e_cblp == 0)
        file_size = (UINT32)hdr->e_cp * 512;
    else
        file_size = (UINT32)(hdr->e_cp - 1) * 512 + hdr->e_cblp;

    Print(L"[DOS] file_size = %u\n", file_size);

    UINT32 header_bytes = (UINT32)hdr->e_cparhdr * 16;
    Print(L"[DOS] header_paragraphs=%u header_bytes=%u\n",
          hdr->e_cparhdr, header_bytes);

    INT32 image_size = (INT32)size - (INT32)header_bytes;

    if (image_size < 0) {
        INT32 tiny_size = (INT32)size - (INT32)header_bytes;
        if (tiny_size < 0)
            tiny_size = 0;

        Print(L"[DOS] EXE tiny detectado, ajustando image_size a %d\n", tiny_size);
        image_size = tiny_size;
    }

    UINT32 max_image = 0;
    if (size > header_bytes)
        max_image = (UINT32)(size - header_bytes);

    if ((UINT32)image_size > max_image) {
        Print(L"[DOS] Ajustando image_size a max_image=%u\n", max_image);
        image_size = (INT32)max_image;
    }

    uint16_t load_seg  = 0x1000;
    uint32_t load_base = load_seg << 4;

    Print(L"[DOS] load_seg=%04x load_base=%08x\n", load_seg, load_base);

    for (UINT32 i = 0; i < (UINT32)image_size; i++)
        mem_write8(load_base + i, 0);

    for (UINT32 i = 0; i < (UINT32)image_size; i++)
        mem_write8(load_base + i, program[header_bytes + i]);

    uint16_t rel_count = hdr->e_crlc;
    uint16_t rel_off   = hdr->e_lfarlc;

    Print(L"[DOS] relocaciones (count=%u, off=%u)\n", rel_count, rel_off);

    const uint8_t *rel_ptr = program + rel_off;

    for (uint16_t i = 0; i < rel_count; i++) {
        uint16_t off = *(uint16_t *)(rel_ptr + i*4 + 0);
        uint16_t seg = *(uint16_t *)(rel_ptr + i*4 + 2);

        uint32_t addr = (load_seg + seg) * 16 + off;
        uint16_t val  = mem_read8(addr) | (mem_read8(addr+1) << 8);

        val += load_seg;

        mem_write8(addr,     val & 0xFF);
        mem_write8(addr + 1, val >> 8);
    }

    uint16_t cs = load_seg + hdr->e_cs;
    uint16_t ip = hdr->e_ip;
    uint16_t ss = load_seg + hdr->e_ss;

    uint16_t sp = hdr->e_sp;
    if (sp == 0)
        sp = 0x0080;

    cpu_set_cs_ip(cs, ip);
    cpu_set_ss_sp(ss, sp);
    cpu_set_ds(cs);
    cpu_set_es(cs);

    Print(L"[DOS] EXE cargado: CS=%04x IP=%04x SS=%04x SP=%04x\n",
          cs, ip, ss, sp);

    Print(L"[DEBUG] Antes del bucle: CS:IP=%04x:%04x\n",
          cpu_get_cs(), cpu_get_ip());

    Print(L"[DOS] Paso 11: bytes en entrypoint\n");
    for (int i = 0; i < 16; i++)
        Print(L"%02x ", mem_read8((cs << 4) + ip + i));
    Print(L"\n");

    Print(L"[DOS] load_exe1: fin\n");
}

void dosbox_run33(const uint8_t *program3, UINTN size3)
{
    Print(L"ENTRANDO EN DOSBOX_RUN 33\n");

    if (!mem)
        dosbox_init();

    //
    // 1. Seleccionar fuente del ejecutable
    //
    if (gExeBuffer != NULL && gExeSize > 0) {
        Print(L"[DOS] Usando EXE cargado desde el FS (%lu bytes)\n", gExeSize);
        program3 = (const uint8_t*)gExeBuffer;
        size3    = (UINTN)gExeSize;
    } else {
        Print(L"[DOS] No hay EXE cargado desde FS, usando embebido\n");
        program3 = noname_exe_3;
        size3    = noname_exe_3_len;
    }

    //
    // 2. Cargar el EXE en la RAM emulada
    //
    Print(L"[DOS] Cargando EXE MZ\n");
    load_com(program3, size3);

    //
    // 3. Ejecutar bucle principal
    //
    Print(L"Ejecutando bucle principal...\n");

    UINT64 steps = 0;
    
    program_exit_flag = false;

    while (!program_exit_flag) {

        //
        //  NUEVO: leer teclado UEFI en cada iteración
        //
        keyboard_poll();
	
        //
        //  NUEVO: si hay tecla, inyectarla al DOS emulado
        //
        if (last_key != 0) {
            dos_keyboard_buffer_push(last_key);  // <-- te lo explico abajo
            last_key = 0;
        }

        //
        // Ejecutar instrucción 8086
        //
        cpu_step();
        steps++;

        if ((steps % 10000) == 0) {
            Print(L"[DOS] steps=%lu CS:IP=%04x:%04x\n",
                  steps, cpu_get_cs(), cpu_get_ip());

	
        }
    }

    Print(L"\n[DOS] Ejecución finalizada (IP=FFFF)\n");
}
void dosbox_run3(const uint8_t *program3, UINTN size3)
{
    Print(L"ENTRANDO EN DOSBOX_RUN 3\n");

    if (!mem)
        dosbox_init();

    //
    // 1. Seleccionar fuente del ejecutable
    //
    if (gExeBuffer != NULL && gExeSize > 0) {
        Print(L"[DOS] Usando EXE cargado desde el FS (%lu bytes)\n", gExeSize);
        program3 = (const uint8_t*)gExeBuffer;
        size3    = (UINTN)gExeSize;
    } else {
        Print(L"[DOS] No hay EXE cargado desde FS, usando embebido\n");
        program3 = noname_exe_3;
        size3    = noname_exe_3_len;
    }

    //
    // 2. Cargar el EXE en la RAM emulada
    //
    Print(L"[DOS] Cargando EXE MZ\n");
    load_exe3(program3, size3);

    //
    // 3. Ejecutar bucle principal
    //
    Print(L"[DOS] Ejecutando bucle principal...\n");

    UINT64 steps = 0;

    while (cpu_get_ip() != 0xFFFF) {

        cpu_step();
        steps++;

        if ((steps % 10000) == 0) {
            Print(L"[DOS] steps=%lu CS:IP=%04x:%04x\n",
                  steps, cpu_get_cs(), cpu_get_ip());
        }
    }

    Print(L"\n[DOS] Ejecución finalizada (IP=FFFF)\n");
}

void dosbox_run2(const uint8_t *program2, UINTN size2)
{
    Print(L"ENTRANDO EN DOSBOX_RUN 2\n");

    if (!mem)
        dosbox_init();

    program2 = noname_exe_2;
    size2 = noname_exe_2_len;
    
    // Cambiar punto de inicializacion
    uint16_t seg = 0x1000;
    uint32_t base = seg << 4;

    // PSP mínimo
    for (int i = 0; i < 256; i++)
        mem[base + i] = 0;

    // Cargar COM en offset 0x100
    for (UINTN i = 0; i < size2; i++)
        mem[base + 0x100 + i] = program2[i];

    // Inicializar registros
    cpu_set_cs_ip(seg, 0x100);
    cpu_set_ds(seg);
    cpu_set_es(seg);
    cpu_set_ss_sp(seg, 0xFFFE);

    Print(L"[DOS] Ejecutando...\n");

    while (cpu_get_ip() != 0xFFFF)
        cpu_step();
    Print(L"\n[DOS] Ejecución finalizada\n");
}

void dosbox_run1(const uint8_t *program, UINTN size)
{
    Print(L"ENTRANDO EN DOSBOX_RUN\n");

    if (!mem)
        dosbox_init();

    program = noname_exe_1;
    size    = noname_exe_1_len;

    Print(L"[DOS] Cargando EXE MZ\n");
    Print(L"[DEBUG] noname_exe_1_len = %lu\n", noname_exe_1_len);

    load_exe1(program, size);

    Print(L"[DOS] Ejecutando bucle principal...\n");

    UINT64 steps = 0;

    while (cpu_get_ip() != 0xFFFF) {

        cpu_step();
        steps++;

        if ((steps % 10000) == 0) {
            Print(L"[DOS] steps=%lu CS:IP=%04x:%04x\n",
                  steps, cpu_get_cs(), cpu_get_ip());
        }
    }

    Print(L"\n[DOS] Ejecución finalizada (IP=FFFF)\n");
}

void run_dos_shell(void)
{
     while (1) { 
       Print(L"[DOS] Cargando COMMAND.COM...\n");

        EFI_STATUS Status = efi_load_by_name(L"COMMAND.COM");
        if (EFI_ERROR(Status)) {
            Print(L"[DOS] ERROR: no se pudo cargar COMMAND.COM\n");
            return;
        }

        // Cargar COMMAND.COM en memoria DOS
        load_com(gExeBuffer, gExeSize);

        // Ejecutar COMMAND.COM
        dosbox_run33(gExeBuffer, gExeSize);

        Print(L"[DOS] COMMAND.COM terminó, reiniciando shell...\n");
     }
}



