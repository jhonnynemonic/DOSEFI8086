#ifndef DOSBOX_CORE_H
#define DOSBOX_CORE_H

#include <efi.h>
#include <efilib.h>
#include <stdint.h>
#include <stdbool.h>
extern uint8_t *mem;
extern uint16_t gComSegment;
void dosbox_init(void);
void dosbox_run1(const uint8_t *program, UINTN size);
void dosbox_run2(const uint8_t *program2, UINTN size2);
void dosbox_run3(const uint8_t *program3, UINTN size3);
void dosbox_run33(const uint8_t *program3, UINTN size3);
void dosbox_runDisk(void);
void load_exe1(const uint8_t *program, UINTN size);
void load_exe3(const uint8_t *program3, UINTN size3);
void load_exe33(const uint8_t *program3, UINTN size3);
void dos_keyboard_buffer_push(uint8_t ascii);
void load_com(const uint8_t *program3, UINTN size3);
void run_dos_shell(void);
#endif


