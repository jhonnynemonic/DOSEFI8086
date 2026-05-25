//#include "../dosbox/stdint.h"
#include <stdint.h>
#include <efi.h>
#include <efilib.h>
#include "../dosbox/dosbox_core.h"
#include "../uefi/uefi_gfx.h"
#include "../uefi/uefi_fs.h"
#include "../uefi/start.h"

EFI_STATUS EFIAPI efi_main(EFI_HANDLE ImageHandle, EFI_SYSTEM_TABLE *SystemTable)
{
    EFI_STATUS Status;

    InitializeLib(ImageHandle, SystemTable);

    Print(L"[UEFI] DOS Loader iniciado\n");

    Print(L"[DBG] efi_fs\n");
    efi_fs(ImageHandle, SystemTable);
    Print(L"[CONSOLE] gfx_init\n");
    gfx_init(ImageHandle, SystemTable);
    Print(L"[DBG] dosbox_run\n");
    dosbox_run(NULL, 0);
    dosbox_run2(NULL, 0);    
    Print(L"[DBG] gfx_present1\n");
    gfx_present2();
    WaitForSingleEvent(ST->ConIn->WaitForKey, 0);
    Print(L"[DBG] Fin limpio\n");



    return EFI_SUCCESS;
}
