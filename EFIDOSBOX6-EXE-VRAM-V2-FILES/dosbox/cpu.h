#ifndef CPU_H
#define CPU_H

#include <efi.h>
#include <efilib.h>

/* Estado mínimo de un 8086 */
typedef struct {
    uint16_t ax, bx, cx, dx;
    uint16_t si, di, bp, sp;
    uint16_t cs, ds, es, ss;
    uint16_t ip;
    uint16_t flags;
} CPUState;

/* Inicialización del CPU con memoria */
void     cpu_reset(void);

/* Acceso al IP */
void     cpu_set_ip(uint16_t ip);
uint16_t cpu_get_ip(void);

/* Lectura del siguiente opcode sin avanzar IP (opcional) */
uint8_t  cpu_peek_opcode(void);

/* Ejecución de una instrucción */
void     cpu_step(void);

/* Funciones necesarias para el loader EXE/COM */
void cpu_set_cs_ip(uint16_t cs, uint16_t ip);
void cpu_set_ss_sp(uint16_t ss, uint16_t sp);
void cpu_set_ds(uint16_t ds);
void cpu_set_es(uint16_t es);



/* Memoria */
void cpu_set_memory(uint8_t *m);

void cpu_set_ds(uint16_t val);
void cpu_set_es(uint16_t val);

extern CPUState cpu;
#endif
