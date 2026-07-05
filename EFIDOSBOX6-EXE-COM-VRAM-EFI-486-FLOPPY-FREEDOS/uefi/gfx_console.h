#ifndef GFX_CONSOLE_H
#define GFX_CONSOLE_H

#include <efi.h>

void gfx_clear(UINT32 color);
void gfx_putc(char c);
void gfx_puts(const char *s);
void gfx_printf(const char *fmt, ...);
void gfx_console_redraw(void);
void gfx_console_redraw(void);
void gfx_console_reset(void);

#endif
