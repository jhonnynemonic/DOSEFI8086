#include <stdint.h>
#include <efi.h>
#include <efilib.h>
#include "../dosbox/cpu_486.h"
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
//  16-BIT VIEW HELPERS (486 REAL MODE)
// ======================================================

static inline uint16_t get_ax(void) { return (uint16_t)(cpu.eax & 0xFFFF); }
static inline uint16_t get_bx(void) { return (uint16_t)(cpu.ebx & 0xFFFF); }
static inline uint16_t get_cx(void) { return (uint16_t)(cpu.ecx & 0xFFFF); }
static inline uint16_t get_dx(void) { return (uint16_t)(cpu.edx & 0xFFFF); }
static inline uint16_t get_si(void) { return (uint16_t)(cpu.esi & 0xFFFF); }
static inline uint16_t get_di(void) { return (uint16_t)(cpu.edi & 0xFFFF); }
static inline uint16_t get_bp(void) { return (uint16_t)(cpu.ebp & 0xFFFF); }
static inline uint16_t get_sp(void) { return (uint16_t)(cpu.esp & 0xFFFF); }
static inline uint16_t get_ip(void) { return (uint16_t)(cpu.eip & 0xFFFF); }

static inline void set_ax(uint16_t v) { cpu.eax = (cpu.eax & 0xFFFF0000) | v; }
static inline void set_bx(uint16_t v) { cpu.ebx = (cpu.ebx & 0xFFFF0000) | v; }
static inline void set_cx(uint16_t v) { cpu.ecx = (cpu.ecx & 0xFFFF0000) | v; }
static inline void set_dx(uint16_t v) { cpu.edx = (cpu.edx & 0xFFFF0000) | v; }
static inline void set_si(uint16_t v) { cpu.esi = (cpu.esi & 0xFFFF0000) | v; }
static inline void set_di(uint16_t v) { cpu.edi = (cpu.edi & 0xFFFF0000) | v; }
static inline void set_bp(uint16_t v) { cpu.ebp = (cpu.ebp & 0xFFFF0000) | v; }
static inline void set_sp(uint16_t v) { cpu.esp = (cpu.esp & 0xFFFF0000) | v; }
static inline void set_ip(uint16_t v) { cpu.eip = (cpu.eip & 0xFFFF0000) | v; }

// ======================================================
//  GETTERS
// ======================================================

uint16_t cpu_get_cs(void) { return cpu.cs; }
uint16_t cpu_get_ip(void) { return get_ip(); }
uint16_t cpu_get_ds(void) { return cpu.ds; }
uint16_t cpu_get_es(void) { return cpu.es; }
uint16_t cpu_get_ss(void) { return cpu.ss; }
uint16_t cpu_get_sp(void) { return get_sp(); }
uint16_t cpu_get_ax(void) { return get_ax(); }
uint16_t cpu_get_dx(void) { return get_dx(); }
uint16_t cpu_get_di(void) { return get_di(); }

// ======================================================
//  SETTERS
// ======================================================

void cpu_set_cs_ip(uint16_t cs, uint16_t ip) {
    cpu.cs = cs;
    set_ip(ip);
}

void cpu_set_ds(uint16_t ds) {
    cpu.ds = ds;
}

void cpu_set_ss_sp(uint16_t ss, uint16_t sp) {
    cpu.ss = ss;
    set_sp(sp);
}

void cpu_set_ax(uint16_t v) {
    set_ax(v);
}

void cpu_set_dx(uint16_t v) {
    set_dx(v);
}

void cpu_set_ip(uint16_t v) {
    set_ip(v);
}

void cpu_set_es(uint16_t es) {
    cpu.es = es;
}

// ======================================================
//  FETCH / PEEK
// ======================================================

static inline uint8_t fetch8(void)
{
    uint32_t addr = (cpu.cs << 4) + get_ip();
    uint8_t v = mem_read8(addr);
    cpu.eip = (cpu.eip + 1) & 0xFFFFFFFF;
    return v;
}

uint8_t cpu_peek_opcode(void)
{
    uint32_t addr = (cpu.cs << 4) + get_ip();
    return mem_read8(addr);
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
    if (v) cpu.eflags |= FLAG_Z;
    else   cpu.eflags &= ~FLAG_Z;
}

static inline void set_flag_C(bool v) {
    if (v) cpu.eflags |= FLAG_C;
    else   cpu.eflags &= ~FLAG_C;
}

static void set_flag(uint16_t flag, BOOLEAN v)
{
    if (v) cpu.eflags |= flag;
    else   cpu.eflags &= ~flag;
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

static void inc16_reg(uint32_t *reg)
{
    uint16_t old = (uint16_t)(*reg & 0xFFFF);
    uint16_t res = old + 1;
    *reg = (*reg & 0xFFFF0000) | res;

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
    cpu.eax = cpu.ebx = cpu.ecx = cpu.edx = 0;
    cpu.esi = cpu.edi = cpu.ebp = 0;
    cpu.esp = 0xFFFE;

    // NO tocar CS, DS, ES, SS, IP (segmentos y parte alta de EIP)
    cpu.eflags = 0x00000200;
}

// ======================================================
//  CPU STEP
// ======================================================

void cpu_step(void)
{
    uint32_t addr = (cpu.cs << 4) + get_ip();
    uint8_t op = mem_read8(addr);

    Print(L"[CPU] cpu_step: CS:IP=%04x:%04x opcode=%02x\n",
          cpu.cs, get_ip(), op);

    cpu.eip = (cpu.eip + 1) & 0xFFFFFFFF;

    switch (op) {

    case 0x90: // NOP
        break;

    // INC reg16
    case 0x40: inc16_reg(&cpu.eax); break;
    case 0x41: inc16_reg(&cpu.ecx); break;
    case 0x42: inc16_reg(&cpu.edx); break;
    case 0x43: inc16_reg(&cpu.ebx); break;
    case 0x44: inc16_reg(&cpu.esp); break;
    case 0x45: inc16_reg(&cpu.ebp); break;
    case 0x46: inc16_reg(&cpu.esi); break;
    case 0x47: inc16_reg(&cpu.edi); break;

    // XOR AX,AX (31 C0)
    case 0x31: {
        uint8_t modrm = mem_read8((cpu.cs << 4) + get_ip());
        cpu.eip = (cpu.eip + 1) & 0xFFFFFFFF;
        if (modrm == 0xC0) {
            uint16_t ax = get_ax();
            ax ^= ax;
            set_ax(ax);
            set_logic_flags16(get_ax());
        }
        break;
    }

    // OR AX, imm16 (0D iw)
    case 0x0D: {
        uint16_t imm = mem_read8((cpu.cs << 4) + get_ip());
        cpu.eip = (cpu.eip + 1) & 0xFFFFFFFF;
        imm |= mem_read8((cpu.cs << 4) + get_ip()) << 8;
        cpu.eip = (cpu.eip + 1) & 0xFFFFFFFF;
        uint16_t ax = get_ax();
        ax |= imm;
        set_ax(ax);
        set_logic_flags16(get_ax());
        break;
    }

    // ADD AX, imm16 (05 iw)
    case 0x05: {
        uint16_t imm = mem_read8((cpu.cs << 4) + get_ip());
        cpu.eip = (cpu.eip + 1) & 0xFFFFFFFF;
        imm |= mem_read8((cpu.cs << 4) + get_ip()) << 8;
        cpu.eip = (cpu.eip + 1) & 0xFFFFFFFF;
        uint16_t old = get_ax();
        uint32_t r32 = (uint32_t)old + imm;
        set_ax((uint16_t)r32);
        set_add_flags16(old, imm, r32);
        break;
    }

    // SUB AX, imm16 (2D iw)
    case 0x2D: {
        uint16_t imm = mem_read8((cpu.cs << 4) + get_ip());
        cpu.eip = (cpu.eip + 1) & 0xFFFFFFFF;
        imm |= mem_read8((cpu.cs << 4) + get_ip()) << 8;
        cpu.eip = (cpu.eip + 1) & 0xFFFFFFFF;
        uint16_t old = get_ax();
        uint32_t r32 = (uint32_t)old - imm;
        set_ax((uint16_t)r32);
        set_sub_flags16(old, imm, r32);
        break;
    }

    // CMP AX, imm16 (3D iw)
    case 0x3D: {
        uint16_t imm = mem_read8((cpu.cs << 4) + get_ip());
        cpu.eip = (cpu.eip + 1) & 0xFFFFFFFF;
        imm |= mem_read8((cpu.cs << 4) + get_ip()) << 8;
        cpu.eip = (cpu.eip + 1) & 0xFFFFFFFF;
        uint16_t ax = get_ax();
        uint32_t r32 = (uint32_t)ax - imm;
        set_sub_flags16(ax, imm, r32);
        break;
    }

    // JZ rel8 (74 cb)
    case 0x74: {
        int8_t rel = (int8_t)mem_read8((cpu.cs << 4) + get_ip());
        cpu.eip = (cpu.eip + 1) & 0xFFFFFFFF;
        if (cpu.eflags & FLAG_ZF)
            set_ip((uint16_t)(get_ip() + rel));
        break;
    }

    // JNZ rel8 (75 cb)
    case 0x75: {
        int8_t rel = (int8_t)mem_read8((cpu.cs << 4) + get_ip());
        cpu.eip = (cpu.eip + 1) & 0xFFFFFFFF;
        if (!(cpu.eflags & FLAG_ZF))
            set_ip((uint16_t)(get_ip() + rel));
        break;
    }

    // JMP rel8 (EB cb)
    case 0xEB: {
        int8_t rel = (int8_t)mem_read8((cpu.cs << 4) + get_ip());
        Print(L"[CPU] JMP corto EB: rel=%d desde IP=%04x\n", rel, get_ip());
        cpu.eip = (cpu.eip + 1) & 0xFFFFFFFF;
        set_ip((uint16_t)(get_ip() + rel));
        Print(L"[CPU] Nuevo IP=%04x\n", get_ip());
        break;
    }

    // INT nn (CD ib)
    case 0xCD: {
        uint8_t intnum = mem_read8((cpu.cs << 4) + get_ip());
        cpu.eip = (cpu.eip + 1) & 0xFFFFFFFF;
        handle_int(intnum);
        break;
    }

    case 0xF3: { // REP prefix
        uint32_t a = (cpu.cs << 4) + get_ip();
        uint8_t next = mem_read8(a);
        cpu.eip = (cpu.eip + 1) & 0xFFFFFFFF;

        switch (next) {

        // REP MOVSB  (F3 A4)
        case 0xA4: {
            while (get_cx() != 0) {
                uint32_t src = (cpu.ds << 4) + get_si();
                uint32_t dst = (cpu.es << 4) + get_di();

                uint8_t v = mem_read8(src);
                mem_write8(dst, v);

                set_si(get_si() + 1);
                set_di(get_di() + 1);
                set_cx(get_cx() - 1);
            }
            return;
        }

        // REP STOSB (F3 AA)
        case 0xAA: {
            while (get_cx() != 0) {
                uint32_t dst = (cpu.es << 4) + get_di();
                mem_write8(dst, get_ax() & 0xFF); // AL

                set_di(get_di() + 1);
                set_cx(get_cx() - 1);
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
        uint8_t al  = get_ax() & 0xFF;
        uint8_t res = al - imm;

        set_flag_Z(res == 0);
        set_flag_C(al < imm);
        break;
    }

    // MOVSB (A4)
    case 0xA4: {
        uint32_t src = (cpu.ds << 4) + get_si();
        uint32_t dst = (cpu.es << 4) + get_di();
        uint8_t v = mem_read8(src);
        mem_write8(dst, v);
        set_si(get_si() + 1);
        set_di(get_di() + 1);
        break;
    }

    // MOV r16, imm16 (B8 + reg)
    case 0xB8: {
        uint16_t imm = mem_read8(addr+1) | (mem_read8(addr+2) << 8);
        set_ax(imm);
        cpu.eip += 2;
        break;
    }
    case 0xB9: {
        uint16_t imm = mem_read8(addr+1) | (mem_read8(addr+2) << 8);
        set_cx(imm);
        cpu.eip += 2;
        break;
    }
    case 0xBA: {
        uint16_t imm = mem_read8(addr+1) | (mem_read8(addr+2) << 8);
        set_dx(imm);
        cpu.eip += 2;
        break;
    }
    case 0xBB: {
        uint16_t imm = mem_read8(addr+1) | (mem_read8(addr+2) << 8);
        set_bx(imm);
        cpu.eip += 2;
        break;
    }
    case 0xBC: {
        uint16_t imm = mem_read8(addr+1) | (mem_read8(addr+2) << 8);
        set_sp(imm);
        cpu.eip += 2;
        break;
    }
    case 0xBD: {
        uint16_t imm = mem_read8(addr+1) | (mem_read8(addr+2) << 8);
        set_bp(imm);
        cpu.eip += 2;
        break;
    }
    case 0xBE: {
        uint16_t imm = mem_read8(addr+1) | (mem_read8(addr+2) << 8);
        set_si(imm);
        cpu.eip += 2;
        break;
    }
    case 0xBF: {
        uint16_t imm = mem_read8(addr+1) | (mem_read8(addr+2) << 8);
        set_di(imm);
        cpu.eip += 2;
        break;
    }

    // MOV r8, imm8 (B0–B7)
    case 0xB0: { // AL
        uint8_t imm = mem_read8(addr+1);
        cpu.eax = (cpu.eax & 0xFFFFFF00) | imm;
        cpu.eip++;
        break;
    }
    case 0xB1: { // CL
        uint8_t imm = mem_read8(addr+1);
        cpu.ecx = (cpu.ecx & 0xFFFFFF00) | imm;
        cpu.eip++;
        break;
    }
    case 0xB2: { // DL
        uint8_t imm = mem_read8(addr+1);
        cpu.edx = (cpu.edx & 0xFFFFFF00) | imm;
        cpu.eip++;
        break;
    }
    case 0xB3: { // BL
        uint8_t imm = mem_read8(addr+1);
        cpu.ebx = (cpu.ebx & 0xFFFFFF00) | imm;
        cpu.eip++;
        break;
    }

    case 0xB4: { // AH
        uint8_t imm = mem_read8(addr+1);
        cpu.eax = (cpu.eax & 0xFFFF00FF) | ((uint32_t)imm << 8);
        cpu.eip++;
        break;
    }
    case 0xB5: { // CH
        uint8_t imm = mem_read8(addr+1);
        cpu.ecx = (cpu.ecx & 0xFFFF00FF) | ((uint32_t)imm << 8);
        cpu.eip++;
        break;
    }
    case 0xB6: { // DH
        uint8_t imm = mem_read8(addr+1);
        cpu.edx = (cpu.edx & 0xFFFF00FF) | ((uint32_t)imm << 8);
        cpu.eip++;
        break;
    }
    case 0xB7: { // BH
        uint8_t imm = mem_read8(addr+1);
        cpu.ebx = (cpu.ebx & 0xFFFF00FF) | ((uint32_t)imm << 8);
        cpu.eip++;
        break;
    }

    case 0x8E: { // MOV Sreg, r/m16
        uint32_t a2 = (cpu.cs << 4) + get_ip();
        uint8_t modrm = mem_read8(a2);
        cpu.eip = (cpu.eip + 1) & 0xFFFFFFFF;

        // Solo caso 8E C0 = mov es, ax
        if (modrm == 0xC0) {
            cpu.es = get_ax();
        } else {
            Print(L"[CPU] MOV Sreg,r/m16 con ModR/M=%02x no implementado\n", modrm);
        }
        return;
    }

    // DEFAULT
    default:
        Print(L"[CPU] Opcode %02x no implementado\n", op);
        set_ip(0xFFFF);
        break;
    }

    // DEBUG CONTEXTO
    {
        uint32_t base = (cpu.cs << 4) + get_ip();

        Print(L"%02x %02x %02x %02x %02x\n",
            mem_read8(base - 2),
            mem_read8(base - 1),
            mem_read8(base + 0),
            mem_read8(base + 1),
            mem_read8(base + 2)
        );

        Print(L"Regs: AX=%04x BX=%04x CX=%04x DX=%04x SI=%04x DI=%04x DS=%04x ES=%04x\n",
              get_ax(), get_bx(), get_cx(), get_dx(),
              get_si(), get_di(), cpu.ds, cpu.es);
    }
}
