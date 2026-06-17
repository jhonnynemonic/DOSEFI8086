//#include "../dosbox/stdint.h"
#include <stdint.h>
#include <efi.h>
#include <efilib.h>
#include "../dosbox/cpu.h"
#include "../uefi/uefi_gfx.h"
#include <stdbool.h>



// ======================================================
//  CPU STATE
// ======================================================


CPUState cpu;

// ======================================================
//  EXTERNAL MEMORY / INTERRUPTS
// ======================================================

extern uint8_t  mem_read8(uint32_t addr);
extern void     mem_write8(uint32_t addr, uint8_t val);
extern void     handle_int(uint8_t intnum);

// ======================================================
//  GETTERS
// ======================================================

uint16_t cpu_get_cs(void) { return cpu.cs; }
uint16_t cpu_get_ip(void) { return cpu.ip; }
uint16_t cpu_get_ds(void) { return cpu.ds; }
uint16_t cpu_get_es(void) { return cpu.es; }
uint16_t cpu_get_ss(void) { return cpu.ss; }
uint16_t cpu_get_sp(void) { return cpu.sp; }
uint16_t cpu_get_ax(void) { return cpu.ax; }
uint16_t cpu_get_dx(void) { return cpu.dx; }
uint16_t cpu_get_di(void) { return cpu.di; }

// ======================================================
//  SETTERS
// ======================================================
void cpu_set_cs_ip(uint16_t cs, uint16_t ip) { 
    cpu.cs = cs; 
    cpu.ip = ip; 
}

void cpu_set_ds(uint16_t ds) { 
    cpu.ds = ds; 
}

void cpu_set_ss_sp(uint16_t ss, uint16_t sp) { 
    cpu.ss = ss; 
    cpu.sp = sp; 
}

void cpu_set_ax(uint16_t v) { 
    cpu.ax = v; 
}

void cpu_set_dx(uint16_t v) { 
    cpu.dx = v; 
}

void cpu_set_ip(uint16_t v) { 
    cpu.ip = v; 
}

void cpu_set_es(uint16_t es) { 
    cpu.es = es; 
}

static inline uint8_t fetch8(void)
{
    uint32_t addr = (cpu.cs << 4) + cpu.ip;
    uint8_t v = mem_read8(addr);
    cpu.ip++;
    return v;
}

// ======================================================
//  FLAGS
// ======================================================

#define FLAG_CF 0x0001
#define FLAG_ZF 0x0040
#define FLAG_SF 0x0080
#define FLAG_OF 0x0800

#define FLAG_Z 0x40
#define FLAG_C 0x01

static inline void set_flag_Z(bool v) {
    if (v) cpu.flags |= FLAG_Z;
    else   cpu.flags &= ~FLAG_Z;
}

static inline void set_flag_C(bool v) {
    if (v) cpu.flags |= FLAG_C;
    else   cpu.flags &= ~FLAG_C;
}


static void set_flag(uint16_t flag, BOOLEAN v)
{
    if (v) cpu.flags |= flag;
    else   cpu.flags &= ~flag;
}

static void set_logic_flags16(uint16_t r)
{
    set_flag(FLAG_CF, FALSE);
    set_flag(FLAG_OF, FALSE);
    set_flag(FLAG_ZF, r == 0);
    set_flag(FLAG_SF, (r & 0x8000) != 0);
}

static void set_add_flags16(uint16_t a, uint16_t b, uint32_t r32)
{
    uint16_t r = (uint16_t)r32;

    set_flag(FLAG_CF, r32 > 0xFFFF);
    set_flag(FLAG_ZF, r == 0);
    set_flag(FLAG_SF, (r & 0x8000) != 0);

    BOOLEAN sa = (a & 0x8000) != 0;
    BOOLEAN sb = (b & 0x8000) != 0;
    BOOLEAN sr = (r & 0x8000) != 0;

    set_flag(FLAG_OF, (sa == sb) && (sa != sr));
}

static void set_sub_flags16(uint16_t a, uint16_t b, uint32_t r32)
{
    uint16_t r = (uint16_t)r32;

    set_flag(FLAG_CF, a < b);
    set_flag(FLAG_ZF, r == 0);
    set_flag(FLAG_SF, (r & 0x8000) != 0);

    BOOLEAN sa = (a & 0x8000) != 0;
    BOOLEAN sb = (b & 0x8000) != 0;
    BOOLEAN sr = (r & 0x8000) != 0;

    set_flag(FLAG_OF, (sa != sb) && (sa != sr));
}

static void inc16(uint16_t *r)
{
    uint16_t old = *r;
    uint16_t res = old + 1;
    *r = res;

    set_flag(FLAG_ZF, res == 0);
    set_flag(FLAG_SF, (res & 0x8000) != 0);

    BOOLEAN so = (old & 0x8000) != 0;
    BOOLEAN sr = (res & 0x8000) != 0;
    set_flag(FLAG_OF, (!so && sr));
}

// ======================================================
//  CPU RESET
// ======================================================

void cpu_reset(void)
{
    cpu.ax = cpu.bx = cpu.cx = cpu.dx = 0;
    cpu.si = cpu.di = cpu.bp = 0;
    cpu.sp = 0xFFFE;

    // NO tocar CS, DS, ES, SS, IP
    cpu.flags = 0x0200;
}

// ======================================================
//  CPU STEP
// ======================================================

void cpu_step(void)
{
    uint32_t addr = (cpu.cs << 4) + cpu.ip;
    uint8_t op = mem_read8(addr);

    Print(L"[CPU] cpu_step: CS:IP=%04x:%04x opcode=%02x\n",
          cpu.cs, cpu.ip, op);

    cpu.ip++;

    switch (op) {

    case 0x90: // NOP
    // no hace nada
    break;
    
    // INC reg16
    case 0x40: inc16(&cpu.ax); break;
    case 0x41: inc16(&cpu.cx); break;
    case 0x42: inc16(&cpu.dx); break;
    case 0x43: inc16(&cpu.bx); break;
    case 0x44: inc16(&cpu.sp); break;
    case 0x45: inc16(&cpu.bp); break;
    case 0x46: inc16(&cpu.si); break;
    case 0x47: inc16(&cpu.di); break;

    // XOR AX,AX (31 C0)
    case 0x31: {
        uint8_t modrm = mem_read8((cpu.cs << 4) + cpu.ip);
        cpu.ip++;
        if (modrm == 0xC0) {
            cpu.ax ^= cpu.ax;
            set_logic_flags16(cpu.ax);
        }
        break;
    }

    // OR AX, imm16 (0D iw)
    case 0x0D: {
        uint16_t imm = mem_read8((cpu.cs << 4) + cpu.ip);
        cpu.ip++;
        imm |= mem_read8((cpu.cs << 4) + cpu.ip) << 8;
        cpu.ip++;
        cpu.ax |= imm;
        set_logic_flags16(cpu.ax);
        break;
    }

    // ADD AX, imm16 (05 iw)
    case 0x05: {
        uint16_t imm = mem_read8((cpu.cs << 4) + cpu.ip);
        cpu.ip++;
        imm |= mem_read8((cpu.cs << 4) + cpu.ip) << 8;
        cpu.ip++;
        uint32_t r32 = (uint32_t)cpu.ax + imm;
        uint16_t old = cpu.ax;
        cpu.ax = (uint16_t)r32;
        set_add_flags16(old, imm, r32);
        break;
    }

    // SUB AX, imm16 (2D iw)
    case 0x2D: {
        uint16_t imm = mem_read8((cpu.cs << 4) + cpu.ip);
        cpu.ip++;
        imm |= mem_read8((cpu.cs << 4) + cpu.ip) << 8;
        cpu.ip++;
        uint32_t r32 = (uint32_t)cpu.ax - imm;
        uint16_t old = cpu.ax;
        cpu.ax = (uint16_t)r32;
        set_sub_flags16(old, imm, r32);
        break;
    }

    // CMP AX, imm16 (3D iw)
    case 0x3D: {
        uint16_t imm = mem_read8((cpu.cs << 4) + cpu.ip);
        cpu.ip++;
        imm |= mem_read8((cpu.cs << 4) + cpu.ip) << 8;
        cpu.ip++;
        uint32_t r32 = (uint32_t)cpu.ax - imm;
        set_sub_flags16(cpu.ax, imm, r32);
        break;
    }

    // JZ rel8 (74 cb)
    case 0x74: {
        int8_t rel = (int8_t)mem_read8((cpu.cs << 4) + cpu.ip);
        cpu.ip++;
        if (cpu.flags & FLAG_ZF)
            cpu.ip = (uint16_t)(cpu.ip + rel);
        break;
    }

    // JNZ rel8 (75 cb)
    case 0x75: {
        int8_t rel = (int8_t)mem_read8((cpu.cs << 4) + cpu.ip);
        cpu.ip++;
        if (!(cpu.flags & FLAG_ZF))
            cpu.ip = (uint16_t)(cpu.ip + rel);
        break;
    }

    // JMP rel8 (EB cb)
    case 0xEB: {
        int8_t rel = (int8_t)mem_read8((cpu.cs << 4) + cpu.ip);
        Print(L"[CPU] JMP corto EB: rel=%d desde IP=%04x\n", rel, cpu.ip);
        cpu.ip++;
        cpu.ip = (uint16_t)(cpu.ip + rel);
        Print(L"[CPU] Nuevo IP=%04x\n", cpu.ip);
        break;
    }

    // INT nn (CD ib)
    case 0xCD: {
        uint8_t intnum = mem_read8((cpu.cs << 4) + cpu.ip);
        cpu.ip++;
        handle_int(intnum);
        break;
    }

    case 0xF3: { // REP prefix
    uint32_t addr = (cpu.cs << 4) + cpu.ip;
    uint8_t next = mem_read8(addr);
    cpu.ip++; // consumir el opcode siguiente

    switch (next) {

    // -------------------------
    // REP MOVSB  (F3 A4)
    // -------------------------
    case 0xA4: {
        while (cpu.cx != 0) {
            uint32_t src = (cpu.ds << 4) + cpu.si;
            uint32_t dst = (cpu.es << 4) + cpu.di;

            uint8_t v = mem_read8(src);
            mem_write8(dst, v);

            cpu.si++;
            cpu.di++;
            cpu.cx--;
        }
        return;
    }

    // -------------------------
    // REP STOSB (F3 AA)
    // -------------------------
    case 0xAA: {
        while (cpu.cx != 0) {
            uint32_t dst = (cpu.es << 4) + cpu.di;
            mem_write8(dst, cpu.ax & 0xFF); // AL

            cpu.di++;
            cpu.cx--;
        }
        return;
    }

    default:
        Print(L"[CPU] REP con opcode %02x no implementado\n", next);
        return;
    }
    }
    case 0x3C: { // CMP AL, imm8
    uint8_t imm = fetch8();
    uint8_t al  = cpu_get_ax() & 0xFF;
    uint8_t res = al - imm;

    // Actualiza flags como SUB:
    set_flag_Z(res == 0);
    set_flag_C(al < imm);

    break;
    }



    // MOVSB (A4)
    case 0xA4: {
        uint32_t src = (cpu.ds << 4) + cpu.si;
        uint32_t dst = (cpu.es << 4) + cpu.di;
        uint8_t v = mem_read8(src);
        mem_write8(dst, v);
        cpu.si++;
        cpu.di++;
        break;
    }

	// MOV r16, imm16 (B8 + reg)
	case 0xB8: cpu.ax = mem_read8(addr+1) | (mem_read8(addr+2) << 8); cpu.ip += 2; break;
	case 0xB9: cpu.cx = mem_read8(addr+1) | (mem_read8(addr+2) << 8); cpu.ip += 2; break;
	case 0xBA: cpu.dx = mem_read8(addr+1) | (mem_read8(addr+2) << 8); cpu.ip += 2; break;
	case 0xBB: cpu.bx = mem_read8(addr+1) | (mem_read8(addr+2) << 8); cpu.ip += 2; break;
	case 0xBC: cpu.sp = mem_read8(addr+1) | (mem_read8(addr+2) << 8); cpu.ip += 2; break;
	case 0xBD: cpu.bp = mem_read8(addr+1) | (mem_read8(addr+2) << 8); cpu.ip += 2; break;
	case 0xBE: cpu.si = mem_read8(addr+1) | (mem_read8(addr+2) << 8); cpu.ip += 2; break;
	case 0xBF: cpu.di = mem_read8(addr+1) | (mem_read8(addr+2) << 8); cpu.ip += 2; break;
	// MOV r8, imm8 (B0–B7)
	case 0xB0: cpu.ax = (cpu.ax & 0xFF00) | mem_read8(addr+1); cpu.ip++; break; // AL
	case 0xB1: cpu.cx = (cpu.cx & 0xFF00) | mem_read8(addr+1); cpu.ip++; break; // CL
	case 0xB2: cpu.dx = (cpu.dx & 0xFF00) | mem_read8(addr+1); cpu.ip++; break; // DL
	case 0xB3: cpu.bx = (cpu.bx & 0xFF00) | mem_read8(addr+1); cpu.ip++; break; // BL

	case 0xB4: cpu.ax = (cpu.ax & 0x00FF) | (mem_read8(addr+1) << 8); cpu.ip++; break; // AH
	case 0xB5: cpu.cx = (cpu.cx & 0x00FF) | (mem_read8(addr+1) << 8); cpu.ip++; break; // CH
	case 0xB6: cpu.dx = (cpu.dx & 0x00FF) | (mem_read8(addr+1) << 8); cpu.ip++; break; // DH
	case 0xB7: cpu.bx = (cpu.bx & 0x00FF) | (mem_read8(addr+1) << 8); cpu.ip++; break; // BH

        case 0x8E: { // MOV Sreg, r/m16
        uint32_t addr = (cpu.cs << 4) + cpu.ip;
        uint8_t modrm = mem_read8(addr);
        cpu.ip++;
    
        // Queremos solo el caso 8E C0 = mov es, ax
        if (modrm == 0xC0) {
        cpu.es = cpu.ax;
        } else {
        Print(L"[CPU] MOV Sreg,r/m16 con ModR/M=%02x no implementado\n", modrm);
        }
        return;
        }


    // DEFAULT
    default:
        Print(L"[CPU] Opcode %02x no implementado\n", op);
        cpu.ip = 0xFFFF;
        break;
    }

    // DEBUG CONTEXTO
    {
        uint32_t base = (cpu.cs << 4) + cpu.ip;

        //Print(L"\n--- CONTEXTO ---\n");
        //Print(L"CS:IP = %04x:%04x\n", cpu.cs, cpu.ip);

        //Print(L"Bytes alrededor:\n");
        Print(L"%02x %02x %02x %02x %02x\n",
            mem_read8(base - 2),
            mem_read8(base - 1),
            mem_read8(base + 0),
            mem_read8(base + 1),
            mem_read8(base + 2)
        );

	Print(L"Regs: AX=%04x BX=%04x CX=%04x DX=%04x SI=%04x DI=%04x DS=%04x ES=%04x\n",
      cpu.ax, cpu.bx, cpu.cx, cpu.dx, cpu.si, cpu.di, cpu.ds, cpu.es);
    }
}
