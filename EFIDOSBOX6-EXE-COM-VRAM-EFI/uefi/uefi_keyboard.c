#include <efi.h>
#include <efilib.h>
#include "uefi_keyboard.h"

static uint8_t dos_keybuf[16];
static int dos_key_head = 0;
static int dos_key_tail = 0;

void dos_keyboard_buffer_push(uint8_t ascii)
{
    int next = (dos_key_head + 1) & 15;
    if (next == dos_key_tail)
        return; // buffer lleno

    dos_keybuf[dos_key_head] = ascii;
    dos_key_head = next;
}

uint8_t dos_keyboard_buffer_pop()
{
    if (dos_key_head == dos_key_tail)
        return 0; // vacío

    uint8_t c = dos_keybuf[dos_key_tail];
    dos_key_tail = (dos_key_tail + 1) & 15;
    return c;
}

EFI_SIMPLE_TEXT_INPUT_EX_PROTOCOL *KeyEx = NULL;

// IMPORTANTE: NO usar gST porque GNU-EFI lo redefine
static EFI_SYSTEM_TABLE *KbdST = NULL;

int last_key = 0;

void keyboard_init(EFI_SYSTEM_TABLE *SystemTable)
{
    KbdST = SystemTable;

    EFI_GUID guid = EFI_SIMPLE_TEXT_INPUT_EX_PROTOCOL_GUID;

    // LocateProtocol usando uefi_call_wrapper
    EFI_STATUS st = uefi_call_wrapper(
        SystemTable->BootServices->LocateProtocol,
        3,
        &guid,
        NULL,
        (void**)&KeyEx
    );

    if (EFI_ERROR(st)) {
        KeyEx = NULL;
    }

    // Reset del teclado simple usando uefi_call_wrapper
    uefi_call_wrapper(
        SystemTable->ConIn->Reset,
        2,
        SystemTable->ConIn,
        FALSE
    );
}
static const char scancode_to_ascii[128] = {
    [0x02] = '1', [0x03] = '2', [0x04] = '3', [0x05] = '4',
    [0x06] = '5', [0x07] = '6', [0x08] = '7', [0x09] = '8',
    [0x0A] = '9', [0x0B] = '0',

    [0x10] = 'q', [0x11] = 'w', [0x12] = 'e', [0x13] = 'r',
    [0x14] = 't', [0x15] = 'y', [0x16] = 'u', [0x17] = 'i',
    [0x18] = 'o', [0x19] = 'p',

    [0x1E] = 'a', [0x1F] = 's', [0x20] = 'd', [0x21] = 'f',
    [0x22] = 'g', [0x23] = 'h', [0x24] = 'j', [0x25] = 'k',
    [0x26] = 'l',

    [0x2C] = 'z', [0x2D] = 'x', [0x2E] = 'c', [0x2F] = 'v',
    [0x30] = 'b', [0x31] = 'n', [0x32] = 'm',
};


void keyboard_poll()
{
    EFI_STATUS Status;
    UINTN Index;

    Status = uefi_call_wrapper(
        KbdST->BootServices->WaitForEvent,
        3,
        1,
        &KbdST->ConIn->WaitForKey,
        &Index
    );
    if (EFI_ERROR(Status))
        return;

    EFI_INPUT_KEY Key;
    Status = uefi_call_wrapper(
        KbdST->ConIn->ReadKeyStroke,
        2,
        KbdST->ConIn,
        &Key
    );
    if (EFI_ERROR(Status))
        return;

    // 1. Ignorar espacios fantasma (no hay tecla real)
    if (Key.UnicodeChar == 0x20 && Key.ScanCode == 0)
        return;

    // 2. Caracteres imprimibles ASCII/Unicode
    if (Key.UnicodeChar >= 0x21 && Key.UnicodeChar <= 0x7E) {
        Print(L"PUSH %c (%02x)\n", Key.UnicodeChar, Key.UnicodeChar);
        dos_keyboard_buffer_push((uint8_t)Key.UnicodeChar);
        return;
    }

    // 3. ENTER
    if (Key.UnicodeChar == 0x0D) {
        dos_keyboard_buffer_push(0x0D);
        return;
    }

    // 4. BACKSPACE
    if (Key.UnicodeChar == 0x08) {
        dos_keyboard_buffer_push(0x08);
        return;
    }
}
