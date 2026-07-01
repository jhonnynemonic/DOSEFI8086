# ============================
#  DOSBox Core UEFI Makefile
# ============================

CC=gcc
LD=ld
OBJCOPY=objcopy
# Rutas GNU‑EFI (WSL)
EFIROOT = ~/gnu-efi-3.0.18

CFLAGS=-I $(EFIROOT)/inc -fpic -ffreestanding -fno-stack-protector -fno-stack-check -fshort-wchar -mno-red-zone -maccumulate-outgoing-args

LDFLAGS =-nostdlib -znocombreloc -shared -Bsymbolic -L $(EFIROOT)/x86_64/lib/ -L $(EFIROOT)/x86_64/gnuefi/ -L $(EFIROOT)/lib -T $(EFIROOT)/gnuefi/elf_x86_64_efi.lds $(EFIROOT)/gnuefi/crt0-efi-x86_64.o


UEFI_OBJS=main.o 
DOSBOX_OBJS=

LIBS = -lgnuefi -lefi 

OBJS = $(UEFI_OBJS) $(DOSBOX_OBJS)

all: bootx64.efi

%.o: %.c
	$(CC) $(CFLAGS) -c $< -o $@

bootx64.so: $(OBJS)
	$(LD) $(LDFLAGS) -o $@ $(OBJS) $(LIBS)

bootx64.efi: bootx64.so
	$(OBJCOPY) -j .text -j .sdata -j .data -j .rodata -j .dynamic -j .dynsym \
    -j .rel -j .rela -j .rel.* -j .rela.* -j .reloc \
    --target efi-app-x86_64 --subsystem=10 $< $@

clean:
	rm -f $(OBJS) bootx64.so bootx64.efi
