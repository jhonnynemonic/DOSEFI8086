#include <efi.h>
#include <efilib.h>
#include "../uefi/gfx_console.h"
#include "../uefi/uefi_gfx.h"   // donde tienes gfx_fb, gfx_pitch, draw_char1()
#include <stdarg.h>

// Tamaño de la pantalla 320x200
#define SCREEN_W 320
#define SCREEN_H 200

// Tamaño de la fuente 8x8
#define CHAR_W 8
#define CHAR_H 8

// Número de columnas y filas
static int con_cols = SCREEN_W / CHAR_W;
static int con_rows = SCREEN_H / CHAR_H;

// Cursor
static int con_x = 0;
static int con_y = 0;

// Color del texto
static UINT32 con_color = 0x00FFFFFF; // blanco

//Buffer
static char console_buffer[4096];
static int console_buffer_len = 0;


// ------------------------------------------------------------
// Limpiar pantalla
// ------------------------------------------------------------
void gfx_clear(UINT32 color)
{
    for (int y = 0; y < SCREEN_H; y++)
        for (int x = 0; x < SCREEN_W; x++)
            gfx_fb[y * gfx_pitch + x] = color;

    con_x = 0;
    con_y = 0;
}

// ------------------------------------------------------------
// Scroll hacia arriba una línea de 8 px
// ------------------------------------------------------------
static void gfx_scroll(void)
{
    int line_bytes = gfx_pitch * CHAR_H;

    // Mover todo hacia arriba
    for (int y = 0; y < SCREEN_H - CHAR_H; y++)
        for (int x = 0; x < SCREEN_W; x++)
            gfx_fb[y * gfx_pitch + x] =
                gfx_fb[(y + CHAR_H) * gfx_pitch + x];

    // Limpiar última línea
    for (int y = SCREEN_H - CHAR_H; y < SCREEN_H; y++)
        for (int x = 0; x < SCREEN_W; x++)
            gfx_fb[y * gfx_pitch + x] = 0x00000000;
}

// ------------------------------------------------------------
// Dibujar un carácter ASCII
// ------------------------------------------------------------
void gfx_putc(char c)
{
    if (console_buffer_len < sizeof(console_buffer)-1)
        console_buffer[console_buffer_len++] = c;

    if (c == '\n') {
        con_x = 0;
        con_y++;
        if (con_y >= con_rows) {
            gfx_scroll();
            con_y = con_rows - 1;
        }
        return;
    }

    int px = con_x * CHAR_W;
    int py = con_y * CHAR_H;

    draw_charbasic(px, py, c, con_color);

    con_x++;
    if (con_x >= con_cols) {
        con_x = 0;
        con_y++;
        if (con_y >= con_rows) {
            gfx_scroll();
            con_y = con_rows - 1;
        }
    }
}

// ------------------------------------------------------------
// Dibujar cadena ASCII
// ------------------------------------------------------------
void gfx_puts(const char *s)
{
    while (*s)
        gfx_putc(*s++);
}

// ------------------------------------------------------------
// printf ASCII → UTF16 → ASCII → pantalla
// ------------------------------------------------------------
#include <efilib.h>   // asegura AsciiVSPrint

void gfx_printf(const char *fmt, ...)
{
    char buf[512];

    va_list args;
    va_start(args, fmt);

    AsciiVSPrint(buf, sizeof(buf), fmt, args);

    va_end(args);

    gfx_puts(buf);
}

void gfx_console_redraw(void)
{
    int saved_x = con_x;
    int saved_y = con_y;

    con_x = 0;
    con_y = 0;

    for (int i = 0; i < console_buffer_len; i++)
        gfx_putc(console_buffer[i]);

    con_x = saved_x;
    con_y = saved_y;
}
void gfx_console_reset(void)
{
    console_buffer_len = 0;
    con_x = 0;
    con_y = 0;
}

