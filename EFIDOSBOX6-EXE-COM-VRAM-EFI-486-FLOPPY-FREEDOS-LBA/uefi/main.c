//#include "../dosbox/stdint.h"
#include <stdint.h>
#include <efi.h>
#include <efilib.h>
#include "../dosbox/dosbox_core.h"
#include "../uefi/uefi_gfx.h"
#include "../uefi/uefi_fs.h"
#include "../uefi/start.h"
#include "../uefi/gfx_console.h"
#include "../uefi/uefi_keyboard.h"

EFI_STATUS EFIAPI efi_main(EFI_HANDLE ImageHandle, EFI_SYSTEM_TABLE *SystemTable)
{
    EFI_STATUS Status;

    InitializeLib(ImageHandle, SystemTable);

    Print(L"[UEFI] DOS Loader iniciado\n");
    // Guardar los handles globales
    gImageHandle = ImageHandle;
    gSystemTable = SystemTable;
    Print(L"[DBG] efi_fs\n");
    efi_fs(ImageHandle, SystemTable);
    efi_fs_disk(ImageHandle, SystemTable);
    efi_fs_stub(ImageHandle, SystemTable);
    Print(L"[CONSOLE] gfx_init\n");
    gfx_init(ImageHandle, SystemTable);
    //Print(L"[DBG] dosbox_run\n");
    //dosbox_run1(NULL, 0);
    //dosbox_run2(NULL, 0);
    //dosbox_run3(NULL, 0);
    //dosbox_run33(NULL, 0);
    gfx_console_reset();
    gfx_clear(0x00000000);
    Print(L"[KDB] OK\n");
    keyboard_init(SystemTable);
    Print(L"[DOS] dosbox_run33 or run_dos_shell\n");
    //dosbox_run33(NULL, 0);
    run_dos_shell();
    //Print(L"[DBG] gfx_console_redraw\n");
    //gfx_console_redraw();    
    //Print(L"[DBG] gfx_present3\n");
    //gfx_present3();
    //gfx_console_redraw();
    //gfx_printf("\n[DOS] Programa finaliado. Pulsa una tecla...\n");
    //WaitForSingleEvent(ST->ConIn->WaitForKey, 0);
    //Print(L"[DBG] gfx_present1\n");
    //gfx_present1();
    //WaitForSingleEvent(ST->ConIn->WaitForKey, 0);
    //Print(L"[DBG] gfx_present2\n");
    //gfx_present2();
    //WaitForSingleEvent(ST->ConIn->WaitForKey, 0);
    Print(L"[DBG] Fin limpio\n");



    return EFI_SUCCESS;
}
