#include <efi.h>
#include <efilib.h>

EFI_SIMPLE_TEXT_INPUT_EX_PROTOCOL *KeyEx = NULL;
extern EFI_GUID SimpleTextInputExProtocol; // OJO: este es el de gnu-efi

EFI_STATUS
EFIAPI
efi_main(EFI_HANDLE ImageHandle, EFI_SYSTEM_TABLE *SystemTable)
{
    InitializeLib(ImageHandle, SystemTable);

    Print(L"UEFI Keyboard Test (SimpleTextInputEx)\n");
    Print(L"Pulsa teclas. ESC para salir.\n\n");

    EFI_STATUS st;

    //
    // Localizar protocolo EX
    //
    st = SystemTable->BootServices->LocateProtocol(
        &SimpleTextInputExProtocol,  // GUID correcto en gnu-efi
        NULL,
        (void**)&KeyEx
    );

    Print(L"LocateProtocol(SimpleTextInputEx) -> %r\n", st);
    Print(L"KeyEx = 0x%lx\n", (UINTN)KeyEx);

    if (EFI_ERROR(st) || !KeyEx) {
        Print(L"No hay SimpleTextInputEx en este firmware.\n");
        return EFI_UNSUPPORTED;
    }

    Print(L"ReadKeyStrokeEx = 0x%lx\n", (UINTN)KeyEx->ReadKeyStrokeEx);

    //
    // Bucle principal
    //
    while (TRUE) {

        EFI_KEY_DATA key;
        st = KeyEx->ReadKeyStrokeEx(KeyEx, &key);

        if (st == EFI_SUCCESS) {

            CHAR16 uni = key.Key.UnicodeChar;
            UINT16 scan = key.Key.ScanCode;

            Print(L"Unicode: 0x%04x  ScanCode: 0x%04x", uni, scan);

            if (uni >= 32 && uni < 127)
                Print(L"  ('%c')\n", uni);
            else
                Print(L"\n");

            if (uni == 0x1B) // ESC
                break;
        }

        SystemTable->BootServices->Stall(1000);
    }

    Print(L"\nSaliendo...\n");
    return EFI_SUCCESS;
}
