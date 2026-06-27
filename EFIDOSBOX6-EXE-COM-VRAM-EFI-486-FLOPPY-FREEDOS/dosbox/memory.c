#include <stdint.h>
#include "../dosbox/memory.h"
#include "../uefi/uefi_gfx.h" //vga_memory

//uint8_t *mem;   // definido en dosbox_core.c

uint8_t mem_read8(uint32_t addr)
{
    // Rango VGA modo 13h: 0xA0000 - 0xA0000 + 320*200
    if (addr >= 0xA0000 && addr < 0xA0000 + 320*200) {
        return vga_memory[addr - 0xA0000];
    }

    if (addr >= 1024*1024)
        return 0;

    return mem[addr];
}

void mem_write8(uint32_t addr, uint8_t val)
{
    // Rango VGA modo 13h
    if (addr >= 0xA0000 && addr < 0xA0000 + 320*200) {
        vga_memory[addr - 0xA0000] = val;
        return;
    }

    if (addr >= 1024*1024)
        return;

    mem[addr] = val;
}

uint16_t mem_read16(uint32_t addr)
{
    uint16_t lo = mem_read8(addr);
    uint16_t hi = mem_read8(addr + 1);
    return lo | (hi << 8);
}

void mem_write16(uint32_t addr, uint16_t val)
{
    mem_write8(addr,     val & 0xFF);
    mem_write8(addr + 1, (val >> 8) & 0xFF);
}
