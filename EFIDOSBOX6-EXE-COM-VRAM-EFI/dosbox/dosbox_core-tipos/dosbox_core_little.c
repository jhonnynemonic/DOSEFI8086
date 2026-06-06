#include <efi.h>
#include <efilib.h>
#include "stdint.h"
#include "memory.h"
#include "int.h"
#include "cpu.h"

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

extern const uint8_t noname_exe[];
extern const UINTN   noname_exe_len;

uint8_t *mem = NULL;

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


void load_exe(const uint8_t *program, UINTN size)
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
    } MZ_HEADER;

    Print(L"[DOS] load_exe: inicio\n");

    if (size < sizeof(MZ_HEADER)) {
        Print(L"[DOS] ERROR: archivo demasiado pequeño\n");
        return;
    }

    const MZ_HEADER *hdr = (const MZ_HEADER *)program;

    if (hdr->e_magic != 0x5A4D) {
        Print(L"[DOS] ERROR: firma MZ incorrecta\n");
        return;
    }

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

    Print(L"[DOS] load_exe: fin\n");
}

void dosbox_run(const uint8_t *program, UINTN size)
{
    Print(L"ENTRANDO EN DOSBOX_RUN\n");

    if (!mem)
        dosbox_init();

    program = noname_exe;
    size    = noname_exe_len;

    Print(L"[DOS] Cargando EXE MZ\n");
    Print(L"[DEBUG] noname_exe_len = %lu\n", noname_exe_len);

    load_exe(program, size);

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
