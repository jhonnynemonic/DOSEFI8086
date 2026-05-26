#ifndef DOSBOX_CORE_H
#define DOSBOX_CORE_H

#include <efi.h>
#include <efilib.h>

void dosbox_init(void);
void dosbox_run(const uint8_t *program, UINTN size);
void dosbox_run2(const uint8_t *program2, UINTN size2);
void dosbox_run3(const uint8_t *program3, UINTN size3);
#endif


