#include <efi.h>
#include <efilib.h>

EFI_STATUS EFIAPI efi_fs(EFI_HANDLE ImageHandle, EFI_SYSTEM_TABLE *SystemTable);
EFI_STATUS EFIAPI efi_ls(EFI_HANDLE ImageHandle, EFI_SYSTEM_TABLE *SystemTable);
EFI_STATUS EFIAPI efi_fs_disk(EFI_HANDLE ImageHandle, EFI_SYSTEM_TABLE *SystemTable);
EFI_STATUS efi_load_by_name(CHAR16 *FileName);

extern UINT32 *gfx_fb;
extern UINTN  gfx_width;
extern UINTN  gfx_height;
extern UINTN  gfx_pitch;
extern uint8_t  *gExeBuffer;
extern UINTN  gExeSize;
extern uint8_t  *gExeBufferDisk;
extern UINTN  gExeSizeDisk;
extern EFI_HANDLE gImageHandle;
extern EFI_SYSTEM_TABLE *gSystemTable;