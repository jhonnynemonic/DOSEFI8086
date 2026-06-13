#include <efi.h>
#include <efilib.h>

EFI_STATUS EFIAPI efi_main(EFI_HANDLE ImageHandle, EFI_SYSTEM_TABLE *SystemTable)
{
    InitializeLib(ImageHandle, SystemTable);

    EFI_INPUT_KEY Key;
    EFI_STATUS Status;
    UINTN Index;

    Print(L"Test de teclado\n");
    Print(L"Presiona una tecla. ESC para salir.\n\n");

    while (1)
    {
        // Esperar evento de teclado
        Status = uefi_call_wrapper(
            SystemTable->BootServices->WaitForEvent,
            3,
            1,
            &SystemTable->ConIn->WaitForKey,
            &Index
        );

        if (EFI_ERROR(Status))
            continue;

        // Leer tecla
        Status = uefi_call_wrapper(
            SystemTable->ConIn->ReadKeyStroke,
            2,
            SystemTable->ConIn,
            &Key
        );

        if (EFI_ERROR(Status))
            continue;

        Print(L"Scancode=0x%04x Unicode=0x%04x\n", Key.ScanCode, Key.UnicodeChar);

        if (Key.ScanCode == SCAN_ESC)
            break;
    }

    return EFI_SUCCESS;
}
