#ifndef CPU_H
#define CPU_H

#include <efi.h>
#include <efilib.h>
#include <stdint.h>
#include <stdbool.h>

/* Estado de un 486 en modo real */
typedef struct {

    /* Registros 32-bit (486) */
    uint32_t eax, ebx, ecx, edx;
    uint32_t esi, edi, ebp, esp;
    uint32_t eip;

    /* Segmentos */
    uint16_t cs, ds, es, ss;

    /* FLAGS completo de 32 bits */
    uint32_t eflags;

} CPUState;

extern CPUState cpu;

/* Inicialización */
void cpu_reset(void);

/* Acceso */
void cpu_set_ip(uint16_t ip);
uint16_t cpu_get_ip(void);

uint8_t  cpu_peek_opcode(void);
void     cpu_step(void);

/* Funciones para loader COM/EXE */
void cpu_set_cs_ip(uint16_t cs, uint16_t ip);
void cpu_set_ss_sp(uint16_t ss, uint16_t sp);
void cpu_set_ds(uint16_t ds);
void cpu_set_es(uint16_t es);

/* Memoria */
void cpu_set_memory(uint8_t *m);

#endif
