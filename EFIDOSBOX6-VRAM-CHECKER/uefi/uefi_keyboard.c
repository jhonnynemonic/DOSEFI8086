#include <efi.h>
#include <efilib.h>
#include "../uefi/uefi_keyboard.h"

EFI_SIMPLE_TEXT_INPUT_EX_PROTOCOL *KeyEx;
int last_key = 0;

void keyboard_init(EFI_SYSTEM_TABLE *SystemTable) {
    EFI_GUID guid = EFI_SIMPLE_TEXT_INPUT_EX_PROTOCOL_GUID;
    EFI_STATUS st = SystemTable->BootServices->LocateProtocol(&guid, NULL, (void**)&KeyEx);

    if (EFI_ERROR(st)) {
        // fallback al teclado simple
        KeyEx = NULL;
    }
}


void keyboard_poll() {
    if (!KeyEx) return; // no bloquear
    EFI_KEY_DATA key;
    if (KeyEx->ReadKeyStrokeEx(KeyEx, &key) == EFI_SUCCESS) {
        last_key = key.Key.UnicodeChar;
    }
}

