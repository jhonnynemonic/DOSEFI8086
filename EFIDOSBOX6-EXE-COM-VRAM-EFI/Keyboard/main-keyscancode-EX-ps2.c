#include <efi.h>
#include <efilib.h>

static inline UINT8 inb(UINT16 port) {
    UINT8 v;
    __asm__ __volatile__("inb %1, %0" : "=a"(v) : "dN"(port));
    return v;
}

EFI_STATUS
EFIAPI
efi_main(EFI_HANDLE ImageHandle, EFI_SYSTEM_TABLE *SystemTable)
{
    InitializeLib(ImageHandle, SystemTable);

    Print(L"UEFI PS/2 Keyboard Scancode Test\n");
    Print(L"Lee directamente puertos 0x60/0x64. ESC para salir.\n\n");

    while (1) {
        // bit 0 de 0x64 = output buffer full
        if (inb(0x64) & 1) {
            UINT8 sc = inb(0x60);
            Print(L"Scancode: 0x%02x\n", sc);

            // ESC make code en set 1 = 0x01
            if (sc == 0x01)
                break;
        }
        SystemTable->BootServices->Stall(1000);
    }

    Print(L"\nSaliendo...\n");
    return EFI_SUCCESS;
}
