#include <efi.h>
#include <efilib.h>
#include "../uefi/uefi_gfx.h"
#pragma pack(push, 1)
typedef struct {
    UINT16 bfType;      // 'BM' = 0x4D42
    UINT32 bfSize;
    UINT16 bfReserved1;
    UINT16 bfReserved2;
    UINT32 bfOffBits;
} BMP_FILE_HEADER;

typedef struct {
    UINT32 biSize;
    INT32  biWidth;
    INT32  biHeight;
    UINT16 biPlanes;
    UINT16 biBitCount;
    UINT32 biCompression;
    UINT32 biSizeImage;
    INT32  biXPelsPerMeter;
    INT32  biYPelsPerMeter;
    UINT32 biClrUsed;
    UINT32 biClrImportant;
} BMP_INFO_HEADER;
#pragma pack(pop)
uint8_t vga_memory[320 * 200];
uint8_t video_mode = 0;

//EFI_GRAPHICS_OUTPUT_PROTOCOL *gop = NULL;
UINTN gfx_width = 0, gfx_height = 0, gfx_pitch = 0;
UINT32 *gfx_fb = NULL;
//EFI_STATUS gfx_init(EFI_SYSTEM_TABLE *SystemTable) {
//    EFI_GUID gopGuid = EFI_GRAPHICS_OUTPUT_PROTOCOL_GUID;
//
//    Print(L"gfx_init: buscando GOP...\n");
//
//    EFI_STATUS st = LibLocateProtocol(&gopGuid, (void**)&gop);
//
//    Print(L"gfx_init: LibLocateProtocol=%r\n", st);
//
//    if (EFI_ERROR(st) || !gop) {
//        Print(L"GOP no encontrado\n");
//        gfx_fb = NULL;
//        gfx_width = gfx_height = 0;
//        return st;
//    }
//
//    gfx_width  = gop->Mode->Info->HorizontalResolution;
//    gfx_height = gop->Mode->Info->VerticalResolution;
//    gfx_fb = (UINT32*)gop->Mode->FrameBufferBase;
//    
//    
//    Print(L"GOP OK %ux%u, FB=%lx\n", gfx_width, gfx_height, gop->Mode->FrameBufferBase);
//    return EFI_SUCCESS;
//}

static EFI_GRAPHICS_OUTPUT_PROTOCOL *gGop = NULL;

static void PutPixel(INT32 x, INT32 y, UINT32 color) {
    if (!gGop) return;
    if (x < 0 || y < 0) return;
    if ((UINT32)x >= gGop->Mode->Info->HorizontalResolution) return;
    if ((UINT32)y >= gGop->Mode->Info->VerticalResolution) return;

    UINT32 *gfx_fb = (UINT32*)gGop->Mode->FrameBufferBase;
    UINT32 pitch = gGop->Mode->Info->PixelsPerScanLine;
    gfx_fb[y * pitch + x] = color;
}
EFI_STATUS EFIAPI gfx_init(EFI_HANDLE ImageHandle, EFI_SYSTEM_TABLE *SystemTable)
{
    EFI_STATUS Status;
    EFI_GUID gopGuid = EFI_GRAPHICS_OUTPUT_PROTOCOL_GUID;

    Status = LibLocateProtocol(&gopGuid, (void**)&gGop);
    if (EFI_ERROR(Status) || !gGop) {
        Print(L"No GOP found: %r\n", Status);
        return Status;
    }

    gfx_width  = gGop->Mode->Info->HorizontalResolution;
    gfx_height = gGop->Mode->Info->VerticalResolution;
    gfx_pitch  = gGop->Mode->Info->PixelsPerScanLine;
    gfx_fb     = (UINT32*)gGop->Mode->FrameBufferBase;

    Print(L"GOP OK %ux%u, pitch=%u, FB=%lx\n",
          gfx_width, gfx_height, gfx_pitch, gfx_fb);

    return EFI_SUCCESS;
}

static const uint8_t font8x8[128][8] = {
    ['L'] = {0x60,0x60,0x60,0x60,0x60,0x60,0x7E,0x00},
    ['E'] = {0x7E,0x60,0x60,0x7C,0x60,0x60,0x7E,0x00},
    ['N'] = {0x66,0x76,0x7E,0x6E,0x66,0x66,0x66,0x00},
    ['O'] = {0x3C,0x66,0x66,0x66,0x66,0x66,0x3C,0x00},
    ['V'] = {0x66,0x66,0x66,0x66,0x66,0x3C,0x18,0x00},
    [' '] = {0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00},
};

static const uint8_t font8x8b[128][8] = {
    ['T'] = {0x7E,0x18,0x18,0x18,0x18,0x18,0x18,0x00},
    ['H'] = {0x66,0x66,0x66,0x7E,0x66,0x66,0x66,0x00},
    ['E'] = {0x7E,0x60,0x60,0x7C,0x60,0x60,0x7E,0x00},
    ['A'] = {0x3C,0x66,0x66,0x7E,0x66,0x66,0x66,0x00},
    [' '] = {0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00},
    ['M'] = {0x63,0x77,0x7F,0x6B,0x63,0x63,0x63,0x00},
};

void draw_char1(int x, int y, char c, UINT32 color) {
    for (int row = 0; row < 8; row++) {
        uint8_t bits = font8x8b[(int)c][row];
        for (int col = 0; col < 8; col++) {
            if (bits & (1 << (7 - col))) {
                //gfx_fb[(y + row) * gfx_width + (x + col)] = color;
		gfx_fb[(y + row) * gfx_pitch + (x + col)] = color;

            }
        }
    }
}

void draw_text1(int x, int y, const char *msg, UINT32 color) {
    while (*msg) {
        draw_char1(x, y, *msg, color);
        x += 8;
        msg++;
    }
}

void gfx_present1() {
    if (video_mode != 0x13) return;

    // 1. Copiar VRAM → framebuffer
    for (int y = 0; y < 200; y++) {
        for (int x = 0; x < 320; x++) {
            uint8_t c = vga_memory[y * 320 + x];

            UINT32 rgb = (c == 4) ? 0x00FF0000 : 0x00000000; //rojo

            //gfx_fb[y * gfx_width + x] = rgb;
	    gfx_fb[y * gfx_pitch + x] = rgb;

        }
    }

    // 2. Dibujar texto centrado en 320x200
    const char *msg = "THE A TEAM";
    int text_w = 10 * 8; // 10 caracteres * 8 px
    int text_x = (320 - text_w) / 2;
    int text_y = (200 - 8) / 2;

    draw_text1(text_x, text_y, msg, 0x00FFFFFF); // blanco
}



void draw_char2(int x, int y, char c, UINT32 color) {
    for (int row = 0; row < 8; row++) {
        uint8_t bits = font8x8[(int)c][row];
        for (int col = 0; col < 8; col++) {
            if (bits & (1 << (7 - col))) {
                //gfx_fb[(y + row) * gfx_width + (x + col)] = color;
		gfx_fb[(y + row) * gfx_pitch + (x + col)] = color;

            }
        }
    }
}

void draw_text2(int x, int y, const char *msg, UINT32 color) {
    while (*msg) {
        draw_char2(x, y, *msg, color);
        x += 8;
        msg++;
    }
}

void gfx_present2() {
    if (video_mode != 0x13) return;

    // 1. Copiar VRAM → framebuffer
    for (int y = 0; y < 200; y++) {
        for (int x = 0; x < 320; x++) {
            uint8_t c = vga_memory[y * 320 + x];

            UINT32 rgb = (c == 4) ? 0x00FF0000 : 0x00000000; //rojo

            //gfx_fb[y * gfx_width + x] = rgb;
            gfx_fb[y * gfx_pitch + x] = rgb;

        }
    }

    // 2. Dibujar texto centrado en 320x200
    const char *msg = "LENOVO";
    int text_w = 6 * 8; // 6 caracteres * 8 px
    int text_x = (320 - text_w) / 2;
    int text_y = (200 - 8) / 2;

    draw_text2(text_x, text_y, msg, 0x00FFFFFF); // blanco
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

