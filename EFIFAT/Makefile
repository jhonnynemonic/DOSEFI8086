# ============================
#  DOSBox Core UEFI Makefile
# ============================

# Rutas GNU‑EFI (WSL)
EFIROOT = ~/gnu-efi-3.0.18

# Toolchain Linux
CC = gcc
LD = ld
OBJCOPY = objcopy

# Flags UEFI-safe
CFLAGS = -I $(EFIROOT)/inc -fpic -ffreestanding -fno-stack-protector -fno-stack-check -fshort-wchar -mno-red-zone -maccumulate-outgoing-args

# Linker flags
LDFLAGS = -shared -Bsymbolic -L $(EFIROOT)/x86_64/lib/ -L $(EFIROOT)/x86_64/gnuefi/ -L $(EFIROOT)/lib -T $(EFIROOT)/gnuefi/elf_x86_64_efi.lds $(EFIROOT)/gnuefi/crt0-efi-x86_64.o

# Object flags
OBFLAGS = -j .text -j .sdata -j .data -j .rodata -j .dynamic -j .dynsym  -j .rel -j .rela -j .rel.* -j .rela.* -j .reloc --target efi-app-x86_64 --subsystem=10

# Library options
LIBS = -lgnuefi -lefi 

# ============================
#  FUENTES
# ============================

CORE_SRCS_C = bootx64.c
CORE_SRCS_O = bootx64.o
CORE_SRCS_SO = bootx64.so
CORE_SRCS_EFI = bootx64.efi

OBJS = $(CORE_SRCS_O) $(CORE_SRCS_SO) $(CORE_SRCS_EFI)

SRCS = $(CORE_SRCS)

# ============================
#  REGLAS
# ============================
all: $(CORE_SRCS_EFI)

$(CORE_SRCS_0): $(CORE_SRCS_C)

	$(CC) $(CFLAGS) -c $(CORE_SRCS_C) -o $(CORE_SRCS_O)
$(CORE_SRCS_SO): $(CORE_SRCS_O)

	$(LD) $(LDFLAGS) $(CORE_SRCS_O) -o $(CORE_SRCS_SO) $(LIBS)

$(CORE_SRCS_EFI): $(CORE_SRCS_SO)

	$(OBJCOPY) $(OBFLAGS) $(CORE_SRCS_SO) $(CORE_SRCS_EFI)

clean:
	rm -f $(CORE_SRCS_O) $(CORE_SRCS_SO) $(CORE_SRCS_EFI)
