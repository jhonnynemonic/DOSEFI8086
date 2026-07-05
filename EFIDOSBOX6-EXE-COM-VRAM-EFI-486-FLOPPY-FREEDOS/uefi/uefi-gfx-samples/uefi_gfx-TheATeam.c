#include <efi.h>
#include <efilib.h>
#include "uefi_gfx.h"

uint8_t vga_memory[320 * 200];
uint8_t video_mode = 0;

EFI_GRAPHICS_OUTPUT_PROTOCOL *gop = NULL;
UINT32 *gfx_fb = NULL;
UINTN gfx_width = 0, gfx_height = 0;

EFI_STATUS gfx_init(EFI_SYSTEM_TABLE *SystemTable) {
    EFI_GUID gopGuid = EFI_GRAPHICS_OUTPUT_PROTOCOL_GUID;

    Print(L"gfx_init: buscando GOP...\n");

    EFI_STATUS st = LibLocateProtocol(&gopGuid, (void**)&gop);

    Print(L"gfx_init: LibLocateProtocol=%r\n", st);

    if (EFI_ERROR(st) || !gop) {
        Print(L"GOP no encontrado\n");
        gfx_fb = NULL;
        gfx_width = gfx_height = 0;
        return st;
    }

    gfx_width  = gop->Mode->Info->HorizontalResolution;
    gfx_height = gop->Mode->Info->VerticalResolution;
    gfx_fb = (UINT32*)gop->Mode->FrameBufferBase;

    Print(L"GOP OK %ux%u, FB=%lx\n", gfx_width, gfx_height, gop->Mode->FrameBufferBase);
    return EFI_SUCCESS;
}
static const uint8_t font8x8[128][8] = {
    ['T'] = {0x7E,0x18,0x18,0x18,0x18,0x18,0x18,0x00},
    ['H'] = {0x66,0x66,0x66,0x7E,0x66,0x66,0x66,0x00},
    ['E'] = {0x7E,0x60,0x60,0x7C,0x60,0x60,0x7E,0x00},
    ['A'] = {0x3C,0x66,0x66,0x7E,0x66,0x66,0x66,0x00},
    [' '] = {0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00},
    ['M'] = {0x63,0x77,0x7F,0x6B,0x63,0x63,0x63,0x00},
};

void draw_char(int x, int y, char c, UINT32 color) {
    for (int row = 0; row < 8; row++) {
        uint8_t bits = font8x8[(int)c][row];
        for (int col = 0; col < 8; col++) {
            if (bits & (1 << (7 - col))) {
                gfx_fb[(y + row) * gfx_width + (x + col)] = color;
            }
        }
    }
}

void draw_text(int x, int y, const char *msg, UINT32 color) {
    while (*msg) {
        draw_char(x, y, *msg, color);
        x += 8;
        msg++;
    }
}

void gfx_present() {
    if (video_mode != 0x13) return;

    // 1. Copiar VRAM → framebuffer
    for (int y = 0; y < 200; y++) {
        for (int x = 0; x < 320; x++) {
            uint8_t c = vga_memory[y * 320 + x];

            UINT32 rgb = (c == 4) ? 0x00FF0000 : 0x00000000; //rojo

            gfx_fb[y * gfx_width + x] = rgb;
        }
    }

    // 2. Dibujar texto centrado en 320x200
    const char *msg = "THE A TEAM";
    int text_w = 10 * 8; // 10 caracteres * 8 px
    int text_x = (320 - text_w) / 2;
    int text_y = (200 - 8) / 2;

    draw_text(text_x, text_y, msg, 0x00FFFFFF); // blanco
}

void uefi_print_char(char c) {
    CHAR16 buf[2];
    buf[0] = (CHAR16)c;
    buf[1] = 0;
    Print(buf);
}

void uefi_print(const char *s) {
    CHAR16 buf[256];
    int i = 0;

    while (*s && i < 255) {
        buf[i++] = (CHAR16)(*s++);
    }
    buf[i] = 0;

    Print(buf);
}
void video_set_mode_13h() {
    // 320x200, 256 colores
    // Inicializa el framebuffer VGA emulado
    for (int i = 0; i < 320*200; i++) {
        vga_memory[i] = 0;   // pantalla negra
    }

    // Si quieres, puedes marcar un flag de modo gráfico
    video_mode = 0x13;
}

