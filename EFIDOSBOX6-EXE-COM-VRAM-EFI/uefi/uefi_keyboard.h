#pragma once
#include <efi.h>

void keyboard_init(EFI_SYSTEM_TABLE *SystemTable);
void keyboard_poll();
extern int last_key;
