This a project DOSEFI and is for show how UEFI can boot ASM code compiled and added to a EFI file with 8086 emulation CPU code adapted to EFI. 
This was compiled wirh GNU-EFI-3.0.18, NASM and Ubuntu WSL, all can be downloaded and installed for try. 
EFIDOSBOX3 EFI File with dosbox.efi named for boot x86_64 8086 emulator and boot .COM of ASM application embebed. 
EFIDOSBOX6 EFI File with dosbos.efi named for boot x86_64 8086 emulator and boot .EXE file of DOS 16-bit type MZ Header embebed.a
EFIDOSBOX6-EXE-COM-VRAM-EFI File for boot emulator 8086 in x86_64 and boot .EXE file and .COM file of DOS 16-bit type MZ Header embebed and start Video Representation in mode 8086 - 13h. File dosbox-LENOVOandTHEATEAM.efi sample with two screens, the change is key press when started on machine or emulator. Importante remember in computer if use SHELL EFI set "mode 80 25" for see the demostratio of emulation screen with 8086 cpu in UEFI computer.
EFIFAT EFI File for view the rootfs of the .IMA File of UltraISO used for boot EFI with CDROM and check access to files.
EFIFAT_BMP EFI File checked access to a .IMA File of UltraISO for UEFI opening a BMP File not compressed. This test is for use in the emulator access to a FS System for load files for emulate disk read.

Now is finally complete de project of DOSEFI8086. Download EFIDOSBOX6-EXE-COM-VRAM-EFI and use the floppy FDD_BOOT.IMA or use the bootx64.efi file adding the file COMMAND.COM en dosbox folder and execute program with intereactive action from EFI to COM's File with UEFI. 
This have a menu of welcome when start and it show COMMAND.COM and COMMAND_KEY.COM. The first start "C>" promt for type "e" and press enter for exit and then, the COM file say "Saliendo de Command" or type "o" for list directories and see files and now you can press "1" for a message of HELLO.COM file in the COMMAND.COM. Also, exist a second program for test the keyboard :).
The option of this program can be, enable features from main.c for start view the other menus was created in development process.

Also i added certs for use the program with UEFI, file pem and key with the der file and crt file and finally pfx for add signed signature. This have not pass its free.

8086 Works! Thanks to IA Copilot.

The intention of this is show how is possible execute old code and emulator in EFI MODE with x86_64. EFI is not only x86_64 EFI with no DOS!! Works with Grub2 2.02.

by PERMA - permao2019w10p@outlook.es

<img width="330" height="169" alt="330px-KL_Intel_D8086" src="https://github.com/user-attachments/assets/319b232a-34e9-4d90-95ea-cbd7e8fab45d" />
