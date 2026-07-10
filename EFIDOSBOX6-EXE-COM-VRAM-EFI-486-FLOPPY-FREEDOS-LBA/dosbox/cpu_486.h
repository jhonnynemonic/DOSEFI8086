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

    uint8_t segment_override;   // 0 = no override, 1 = override activo
    uint16_t override_seg; 
} CPUState;

extern CPUState cpu;
// 8-bit register helpers
uint8_t  get_al(void);
uint8_t  get_ah(void);
void     set_al(uint8_t v);
void     set_ah(uint8_t v);

// 16-bit register helpers
uint16_t get_ax(void);
uint16_t get_bx(void);
uint16_t get_cx(void);
uint16_t get_dx(void);
uint16_t get_si(void);
uint16_t get_di(void);
uint16_t get_bp(void);
uint16_t get_sp(void);
uint16_t get_ip(void);

void     set_ax(uint16_t v);
void     set_bx(uint16_t v);
void     set_cx(uint16_t v);
void     set_dx(uint16_t v);
void     set_si(uint16_t v);
void     set_di(uint16_t v);
void     set_bp(uint16_t v);
void     set_sp(uint16_t v);
void     set_ip(uint16_t v);




/* Inicialización */
void cpu_reset(void);

/* Acceso */
void cpu_set_ip(uint16_t ip);
uint16_t cpu_get_ip(void);

uint8_t  cpu_peek_opcode(void);
void     cpu_step(void);

/* Funciones para loader COM/EXE */

void cpu_set_ah(uint8_t v);
void cpu_set_al(uint8_t v);

void cpu_set_cs_ip(uint16_t cs, uint16_t ip);
void cpu_set_ds(uint16_t ds);
void cpu_set_ss_sp(uint16_t ss, uint16_t sp);
void cpu_set_ax(uint16_t v);
void cpu_set_dx(uint16_t v);
void cpu_set_ip(uint16_t v);
void cpu_set_es(uint16_t es);
void cpu_set_sp(uint16_t v);   // ← AÑADIR ESTE
void cpu_set_ah(uint8_t v);
void cpu_set_al(uint8_t v);


/* Memoria */
void cpu_set_memory(uint8_t *m);

#define FLAG_CF 0x0001
#define FLAG_PF 0x0004
#define FLAG_AF 0x0010
#define FLAG_ZF 0x0040
#define FLAG_SF 0x0080
#define FLAG_TF 0x0100
#define FLAG_IF 0x0200
#define FLAG_DF 0x0400
#define FLAG_OF 0x0800

#define FLAG_Z 0x40
#define FLAG_C 0x01
void cpu_interrupt(uint8_t intnum);
void cpu_iret(void);
void dos_boot_init(void);
void init_ivt(void);

#endif
