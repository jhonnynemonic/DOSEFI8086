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

    EFI_FILE_INFO *FileInfo;
    UINTN BufferSize = 4096;

    // Primera llamada para obtener el tamaño necesario

    FileInfo = AllocatePool(BufferSize);
    if (!FileInfo) {
        Print(L"[ERR] No hay memoria\n");
        return EFI_OUT_OF_RESOURCES;
    }

    while (TRUE) {
        UINTN Size = BufferSize;
        Status = uefi_call_wrapper(Root->Read, 3, Root, &Size, FileInfo);

        if (EFI_ERROR(Status)){
           Print(L"Error de Status");
           return Status;
           break; 
        }
        // Saltar entradas vacías
        if (FileInfo->FileName[0] == L'\0')
            continue;

        if (FileInfo->Attribute & EFI_FILE_DIRECTORY)
            Print(L"[DIR]  %s\n", FileInfo->FileName) && Print(L"ATTR = %lx\n", FileInfo->Attribute);
        
        if (FileInfo->Attribute & EFI_FILE_HIDDEN)
            Print(L"[HIDDEN]  %s\n", FileInfo->FileName) &&  Print(L"ATTR = %lx\n", FileInfo->Attribute);
           
        if (FileInfo->Attribute & EFI_FILE_SYSTEM)
            Print(L"[SYSTEM]  %s\n", FileInfo->FileName) &&  Print(L"ATTR = %lx\n", FileInfo->Attribute);
           
        if (FileInfo->Attribute & EFI_FILE_RESERVIED)
            Print(L"[RESERVIED]  %s\n", FileInfo->FileName) &&  Print(L"ATTR = %lx\n", FileInfo->Attribute);
           
        if (FileInfo->Attribute & EFI_FILE_ARCHIVE)
            Print(L"[ARCHIVE]  %s\n", FileInfo->FileName) &&  Print(L"ATTR = %lx\n", FileInfo->Attribute);
           
        if (Size == 0) {
            break;
        }
 
    }

    FreePool(FileInfo);

    EFI_FILE_PROTOCOL *File;
    Status = uefi_call_wrapper(Root->Open, 5,
                               Root,
                               &File,
                               L"image.bmp",
                               EFI_FILE_MODE_READ,
                               0);
    if (EFI_ERROR(Status)) {
        Print(L"Cannot open image.bmp\n");
        return Status;
    }

    if (!EFI_ERROR(Status)) {
        Print(L"Opened image.bmp\n");
       
    }

// Leer cabeceras BMP
    BMP_FILE_HEADER fileHeader;
    BMP_INFO_HEADER infoHeader;
    UINTN Size;

    Size = sizeof(BMP_FILE_HEADER);
    Status = uefi_call_wrapper(File->Read, 3, File, &Size, &fileHeader);
    if (EFI_ERROR(Status) || Size != sizeof(BMP_FILE_HEADER)) {
        Print(L"Error reading BMP file header\n");
        File->Close(File);
        return EFI_LOAD_ERROR;
    }

    Size = sizeof(BMP_INFO_HEADER);
    Status = uefi_call_wrapper(File->Read, 3, File, &Size, &infoHeader);
    if (EFI_ERROR(Status) || Size != sizeof(BMP_INFO_HEADER)) {
        Print(L"Error reading BMP info header\n");
        File->Close(File);
        return EFI_LOAD_ERROR;
    }

    // Validar BMP
    if (fileHeader.bfType != 0x4D42) { // 'BM'
        Print(L"Not a BMP file\n");
        File->Close(File);
        return EFI_LOAD_ERROR;
    }

    if (infoHeader.biBitCount != 24 || infoHeader.biCompression != 0) {
        Print(L"Only 24-bit uncompressed BMP supported\n");
        File->Close(File);
        return EFI_LOAD_ERROR;
    }

    INT32 width  = infoHeader.biWidth;
    INT32 height = infoHeader.biHeight;
    if (width <= 0 || height == 0) {
        Print(L"Unsupported BMP dimensions\n");
        File->Close(File);
        return EFI_LOAD_ERROR;
    }

    // Saltar hasta bfOffBits (por si hay paletas, etc.)
    UINTN currentPos;
    {
        EFI_FILE_GET_POSITION pos;
        // No hay API directa para leer posición, así que reposicionamos
        // a bfOffBits desde el principio.
        Status = uefi_call_wrapper(File->SetPosition, 2, File, fileHeader.bfOffBits);
        if (EFI_ERROR(Status)) {
            Print(L"SetPosition failed\n");
            File->Close(File);
            return Status;
        }
    }

    // Calcular tamaño de una fila (alineada a 4 bytes)
    UINTN rowSize = ((width * 3 + 3) & ~3);

    // Reservar buffer para una fila
    UINT8 *rowBuffer;
    Status = uefi_call_wrapper(BS->AllocatePool, 3,
                               EfiLoaderData,
                               rowSize,
                               (VOID**)&rowBuffer);
    if (EFI_ERROR(Status)) {
        Print(L"AllocatePool failed\n");
        File->Close(File);
        return Status;
    }

    // Dibujar BMP (BMP 24-bit suele estar almacenado bottom-up si height > 0)
    BOOLEAN bottomUp = (height > 0);
    INT32 absHeight = (height > 0) ? height : -height;

    for (INT32 y = 0; y < absHeight; y++) {
        Size = rowSize;
        Status = uefi_call_wrapper(File->Read, 3, File, &Size, rowBuffer);
        if (EFI_ERROR(Status) || Size != rowSize) {
            Print(L"Error reading BMP data\n");
            break;
        }

        INT32 destY = bottomUp ? (absHeight - 1 - y) : y;

        UINT8 *p = rowBuffer;
        for (INT32 x = 0; x < width; x++) {
            UINT8 b = *p++;
            UINT8 g = *p++;
            UINT8 r = *p++;
            UINT32 color = (r << 16) | (g << 8) | b;
            PutPixel(x, destY, color);
        }
    }

    uefi_call_wrapper(BS->FreePool, 1, rowBuffer);
    File->Close(File);

    Print(L"image.bmp mostrado. Pulsa una tecla...\n");
    WaitForSingleEvent(ST->ConIn->WaitForKey, 0);


    return EFI_SUCCESS;
}


