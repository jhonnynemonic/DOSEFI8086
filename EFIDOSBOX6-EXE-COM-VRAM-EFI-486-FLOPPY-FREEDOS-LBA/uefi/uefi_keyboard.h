#pragma once
#include <efi.h>

void keyboard_init(EFI_SYSTEM_TABLE *SystemTable);
void keyboard_poll();
void dos_keyboard_buffer_push(uint8_t ascii);
uint8_t dos_keyboard_buffer_pop();
extern int last_key;
extern EFI_SYSTEM_TABLE *KbdST;