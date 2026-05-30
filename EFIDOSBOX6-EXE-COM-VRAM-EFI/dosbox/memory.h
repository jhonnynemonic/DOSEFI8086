#ifndef DOSBOX_MEMORY_H
#define DOSBOX_MEMORY_H

#include <stdint.h>

extern uint8_t  mem_read8(uint32_t addr);
extern void     mem_write8(uint32_t addr, uint8_t val);

extern uint8_t *mem;   // si usas el buffer global de memoria

#endif
