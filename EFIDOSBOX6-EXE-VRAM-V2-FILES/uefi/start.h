#include <efi.h>
#include <efilib.h>

EFI_STATUS EFIAPI efi_fs(EFI_HANDLE ImageHandle, EFI_SYSTEM_TABLE *SystemTable);

extern UINT32 *gfx_fb;
extern UINTN  gfx_width;
extern UINTN  gfx_height;
extern UINTN  gfx_pitch;