#pragma once
#include <efi.h>
#include <efilib.h>
#include <stdint.h>

EFI_STATUS EFIAPI gfx_init(EFI_HANDLE ImageHandle, EFI_SYSTEM_TABLE *SystemTable);

extern UINT32 *gfx_fb;
extern UINTN  gfx_width;
extern UINTN  gfx_height;
extern UINTN  gfx_pitch;

extern uint8_t vga_memory[320 * 200];
extern uint8_t video_mode;

void video_set_mode_13h(void);

void gfx_present1(void);
void gfx_present2(void);
void gfx_present3(void);

void uefi_print_char(char c);
void uefi_print(const char *s);

void draw_char1(int x, int y, char c, UINT32 color);
void draw_text1(int x, int y, const char *msg, UINT32 color);
void draw_char2(int x, int y, char c, UINT32 color);
void draw_text2(int x, int y, const char *msg, UINT32 color);
void draw_charbasic(int x, int y, char c, UINT32 color);
