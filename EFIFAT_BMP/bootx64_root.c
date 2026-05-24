#include <efi.h>
#include <efilib.h>

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

static EFI_GRAPHICS_OUTPUT_PROTOCOL *gGop = NULL;

static void PutPixel(INT32 x, INT32 y, UINT32 color) {
    if (!gGop) return;
    if (x < 0 || y < 0) return;
    if ((UINT32)x >= gGop->Mode->Info->HorizontalResolution) return;
    if ((UINT32)y >= gGop->Mode->Info->VerticalResolution) return;

    UINT32 *fb = (UINT32*)gGop->Mode->FrameBufferBase;
    UINT32 pitch = gGop->Mode->Info->PixelsPerScanLine;
    fb[y * pitch + x] = color;
}

EFI_STATUS EFIAPI efi_main(EFI_HANDLE ImageHandle, EFI_SYSTEM_TABLE *SystemTable) {
    InitializeLib(ImageHandle, SystemTable);

    EFI_STATUS Status;

    // Localizar GOP
    {
        UINTN HandleCount = 0;
        EFI_HANDLE *HandleBuffer = NULL;
        Status = uefi_call_wrapper(BS->LocateHandleBuffer, 5,
                                   ByProtocol,
                                   &gEfiGraphicsOutputProtocolGuid,
                                   NULL,
                                   &HandleCount,
                                   &HandleBuffer);
        if (EFI_ERROR(Status) || HandleCount == 0) {
            Print(L"No GOP found\n");
            return Status;
        }

        Status = uefi_call_wrapper(BS->HandleProtocol, 3,
                                   HandleBuffer[0],
                                   &gEfiGraphicsOutputProtocolGuid,
                                   (VOID**)&gGop);
        if (EFI_ERROR(Status)) {
            Print(L"HandleProtocol GOP failed\n");
            return Status;
        }
    }

    // Obtener LoadedImage para saber el dispositivo
    EFI_LOADED_IMAGE *LoadedImage;
    Status = uefi_call_wrapper(BS->HandleProtocol, 3,
                               ImageHandle,
                               &gEfiLoadedImageProtocolGuid,
                               (VOID**)&LoadedImage);
    if (EFI_ERROR(Status)) {
        Print(L"HandleProtocol LoadedImage failed\n");
        return Status;
    }

    // Obtener SimpleFileSystem del mismo dispositivo
    EFI_SIMPLE_FILE_SYSTEM_PROTOCOL *SimpleFS;
    Status = uefi_call_wrapper(BS->HandleProtocol, 3,
                               LoadedImage->DeviceHandle,
                               &gEfiSimpleFileSystemProtocolGuid,
                               (VOID**)&SimpleFS);
    if (EFI_ERROR(Status)) {
        Print(L"SimpleFileSystem not found\n");
        return Status;
    }

    EFI_FILE_PROTOCOL *Root;
    Status = uefi_call_wrapper(SimpleFS->OpenVolume, 2, SimpleFS, &Root);
    if (EFI_ERROR(Status)) {
        Print(L"OpenVolume failed\n");
        return Status;
    }

    Print(L"=== LISTADO DE FICHEROS EN LA RAIZ ===\n");

    return EFI_SUCCESS;
}
