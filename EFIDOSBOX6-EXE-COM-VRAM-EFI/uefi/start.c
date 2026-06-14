#include <efi.h>
#include <efilib.h>
#include "../uefi/start.h"
#include "../uefi/gfx_console.h"

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
EFI_HANDLE gImageHandle = NULL;
EFI_SYSTEM_TABLE *gSystemTable = NULL;

// Buffer global para el ejecutable DOS
VOID  *gExeBuffer = NULL;
UINTN  gExeSize   = 0;

static void PutPixel(INT32 x, INT32 y, UINT32 color) {
    if (!gGop) return;
    if (x < 0 || y < 0) return;
    if ((UINT32)x >= gGop->Mode->Info->HorizontalResolution) return;
    if ((UINT32)y >= gGop->Mode->Info->VerticalResolution) return;

    UINT32 *fb = (UINT32*)gGop->Mode->FrameBufferBase;
    UINT32 pitch = gGop->Mode->Info->PixelsPerScanLine;
    fb[y * pitch + x] = color;
}

static EFI_STATUS
LoadFileToMemory(EFI_FILE_PROTOCOL *File, VOID **OutBuffer, UINTN *OutSize) {
    EFI_STATUS Status;
    EFI_FILE_INFO *Info = NULL;
    UINTN InfoSize = 0;

    // 1. Obtener tamaño del fichero con GetInfo
    Status = uefi_call_wrapper(File->GetInfo, 4,
                               File,
                               &gEfiFileInfoGuid,
                               &InfoSize,
                               NULL);
    if (Status != EFI_BUFFER_TOO_SMALL) {
        return Status;
    }

    Info = AllocatePool(InfoSize);
    if (!Info) {
        return EFI_OUT_OF_RESOURCES;
    }

    Status = uefi_call_wrapper(File->GetInfo, 4,
                               File,
                               &gEfiFileInfoGuid,
                               &InfoSize,
                               Info);
    if (EFI_ERROR(Status)) {
        FreePool(Info);
        return Status;
    }

    UINTN FileSize = (UINTN)Info->FileSize;
    FreePool(Info);

    // 2. Reservar memoria
    VOID *Buffer = AllocatePool(FileSize);
    if (!Buffer) {
        return EFI_OUT_OF_RESOURCES;
    }

    // 3. Leer desde el principio
    Status = uefi_call_wrapper(File->SetPosition, 2, File, 0);
    if (EFI_ERROR(Status)) {
        FreePool(Buffer);
        return Status;
    }

    UINTN ReadSize = FileSize;
    Status = uefi_call_wrapper(File->Read, 3, File, &ReadSize, Buffer);
    if (EFI_ERROR(Status) || ReadSize != FileSize) {
        FreePool(Buffer);
        return EFI_DEVICE_ERROR;
    }

    *OutBuffer = Buffer;
    *OutSize   = FileSize;
    return EFI_SUCCESS;
}
EFI_STATUS EFIAPI efi_ls(EFI_HANDLE ImageHandle, EFI_SYSTEM_TABLE *SystemTable) {

    EFI_STATUS Status;
    gImageHandle = ImageHandle;
    gSystemTable = SystemTable;
    // Obtener LoadedImage
    EFI_LOADED_IMAGE_PROTOCOL *LoadedImage;
    Status = uefi_call_wrapper(BS->HandleProtocol, 3,
                               ImageHandle,
                               &gEfiLoadedImageProtocolGuid,
                               (VOID**)&LoadedImage);
    if (EFI_ERROR(Status)) {
        Print(L"[efi_ls] HandleProtocol LoadedImage failed\n");
        return Status;
    }

    // Obtener SimpleFS
    EFI_SIMPLE_FILE_SYSTEM_PROTOCOL *SimpleFS;
    Status = uefi_call_wrapper(BS->HandleProtocol, 3,
                               LoadedImage->DeviceHandle,
                               &gEfiSimpleFileSystemProtocolGuid,
                               (VOID**)&SimpleFS);
    if (EFI_ERROR(Status)) {
        Print(L"[efi_ls] SimpleFileSystem not found\n");
        return Status;
    }

    EFI_FILE_PROTOCOL *Root;
    Status = uefi_call_wrapper(SimpleFS->OpenVolume, 2, SimpleFS, &Root);
    if (EFI_ERROR(Status)) {
        Print(L"[efi_ls] OpenVolume failed\n");
        return Status;
    }

    
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
                if (Size == 0) {
            break;
        }
        // Saltar entradas vacías
        if (FileInfo->FileName[0] == L'\0')
            continue;

        if (FileInfo->Attribute & EFI_FILE_DIRECTORY){
            gfx_printf("[DIR]  %s\n", FileInfo->FileName);
            gfx_printf("ATTR = %lx\n", FileInfo->Attribute);
        }
        if (FileInfo->Attribute & EFI_FILE_HIDDEN){
            gfx_printf("[HIDDEN]  %s\n", FileInfo->FileName);
            gfx_printf("ATTR = %lx\n", FileInfo->Attribute);
        }           
        if (FileInfo->Attribute & EFI_FILE_SYSTEM){
            gfx_printf("[SYSTEM]  %s\n", FileInfo->FileName); 
            gfx_printf("ATTR = %lx\n", FileInfo->Attribute);
        }   
        if (FileInfo->Attribute & EFI_FILE_RESERVIED){
            gfx_printf("[RESERVIED]  %s\n", FileInfo->FileName); 
            gfx_printf("ATTR = %lx\n", FileInfo->Attribute);
        }   
        if (FileInfo->Attribute & EFI_FILE_ARCHIVE){
            gfx_printf("[ARCHIVE]  %s\n", FileInfo->FileName);
            gfx_printf("ATTR = %lx\n", FileInfo->Attribute);
        }   

 
    }

    FreePool(FileInfo);
}

EFI_STATUS EFIAPI efi_fs(EFI_HANDLE ImageHandle, EFI_SYSTEM_TABLE *SystemTable) {
    EFI_STATUS Status;

    // Localizar GOP (por si quieres dibujar algo luego)
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
            // No es crítico para cargar el fichero, así que no salimos en error duro
        } else {
            Status = uefi_call_wrapper(BS->HandleProtocol, 3,
                                       HandleBuffer[0],
                                       &gEfiGraphicsOutputProtocolGuid,
                                       (VOID**)&gGop);
            if (EFI_ERROR(Status)) {
                Print(L"HandleProtocol GOP failed\n");
                gGop = NULL;
            }
        }
    }

    // Obtener LoadedImage para saber el dispositivo
    EFI_LOADED_IMAGE_PROTOCOL *LoadedImage;
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

    // --- Aquí ya vamos a lo que queremos: cargar NONAME.EXE en memoria ---

    EFI_FILE_PROTOCOL *File;
    CHAR16 *FileName = L"COMMAND.COM";   // cámbialo si quieres otro nombre

    Status = uefi_call_wrapper(Root->Open, 5,
                               Root,
                               &File,
                               FileName,
                               EFI_FILE_MODE_READ,
                               0);
    if (EFI_ERROR(Status)) {
        Print(L"No se pudo abrir %s (Status=%r)\n", FileName, Status);
        return Status;
    }

    // Cargar el fichero entero en memoria
    Status = LoadFileToMemory(File, &gExeBuffer, &gExeSize);
    uefi_call_wrapper(File->Close, 1, File);

    if (EFI_ERROR(Status)) {
        Print(L"Error cargando %s en memoria (Status=%r)\n", FileName, Status);
        gExeBuffer = NULL;
        gExeSize   = 0;
        return Status;
    }

    Print(L"Cargado %s en memoria: %lu bytes\n", FileName, (UINT64)gExeSize);

    // Aquí ya puedes llamar a dosbox_core_run() o similar desde fuera,
    // usando gExeBuffer y gExeSize como fuente del programa.

    return EFI_SUCCESS;
}
