This a project DOSEFI and is for show how UEFI can boot ASM code compiled and added to a EFI file with 8086 emulation CPU code adapted to EFI. 
This was compiled wirh GNU-EFI-3.0.18, NASM and Ubuntu WSL, all can be downloaded and installed for try. 
EFIDOSBOX3 EFI File with dosbox.efi named for boot x86_64 8086 emulator and boot .COM of ASM application embebed. 
EFIDOSBOX6 EFI File with dosbos.efi named for boot x86_64 8086 emulator and boot .EXE file of DOS 16-bit type MZ Header embebed.a
EFIDOSBOX6-EXE-VRAM EFI File and ISO for boot emulator 8086 in x86_64 and boot .EXE file of DOS 16-bit type MZ Header embebed and start Video Representation in mode 8086 - 13h. File dosbox-LENOVOandTHEATEAM.efi sample with two screens, the change is key press when started on machine or emulator. Importante remember in computer if use SHELL EFI set "mode 80 25" for see the demostratio of emulation screen with 8086 cpu in UEFI computer.
EFIFAT EFI File for view the rootfs of the .IMA File of UltraISO used for boot EFI with CDROM and check access to files.
EFIFAT_BMP EFI File checked access to a .IMA File of UltraISO for UEFI opening a BMP File not compressed. This test is for use in the emulator access to a FS System for load files for emulate disk read.
EFIDOSBOX-EXE-VRAM-V2-FILE EFI File with floppy UltraISO access for load in emulator the files and execute from Floppy.
EFIDOSBOX-EXE-VRAM-V2-FILE-SD name 8086SD is the EFI File with floppy UltraISIO read accees and load of noname_exe.c, noname_exe_2.c and noname_exe3.bin for execute MZ DOS FILE 16-bit with no extra content in the headers using GNU-EFI-3.0.18 complete proccess with 3 messages in execution on.
The intention of this is show how is possible execute old code and emulator in EFI MODE with x86_64. EFI is not only x86_64 EFI with no DOS!! Works with Grub2 2.02.
