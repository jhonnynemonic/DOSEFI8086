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
extern uint16_t mem_read16(uint32_t addr);
extern void mem_write16(uint32_t addr, uint16_t val);

// ======================================================
//  16-BIT VIEW HELPERS (486 REAL MODE)
// ======================================================

uint16_t get_ax(void) { return (uint16_t)(cpu.eax & 0xFFFF); }
uint16_t get_bx(void) { return (uint16_t)(cpu.ebx & 0xFFFF); }
uint16_t get_cx(void) { return (uint16_t)(cpu.ecx & 0xFFFF); }
uint16_t get_dx(void) { return (uint16_t)(cpu.edx & 0xFFFF); }
uint16_t get_si(void) { return (uint16_t)(cpu.esi & 0xFFFF); }
uint16_t get_di(void) { return (uint16_t)(cpu.edi & 0xFFFF); }
uint16_t get_bp(void) { return (uint16_t)(cpu.ebp & 0xFFFF); }
uint16_t get_sp(void) { return (uint16_t)(cpu.esp & 0xFFFF); }
uint16_t get_ip(void) { return (uint16_t)(cpu.eip & 0xFFFF); }

void set_ax(uint16_t v) { cpu.eax = (cpu.eax & 0xFFFF0000) | v; }
void set_bx(uint16_t v) { cpu.ebx = (cpu.ebx & 0xFFFF0000) | v; }
void set_cx(uint16_t v) { cpu.ecx = (cpu.ecx & 0xFFFF0000) | v; }
void set_dx(uint16_t v) { cpu.edx = (cpu.edx & 0xFFFF0000) | v; }
void set_si(uint16_t v) { cpu.esi = (cpu.esi & 0xFFFF0000) | v; }
void set_di(uint16_t v) { cpu.edi = (cpu.edi & 0xFFFF0000) | v; }
void set_bp(uint16_t v) { cpu.ebp = (cpu.ebp & 0xFFFF0000) | v; }
void set_sp(uint16_t v) { cpu.esp = (cpu.esp & 0xFFFF0000) | v; }
void set_ip(uint16_t v) { cpu.eip = (cpu.eip & 0xFFFF0000) | v; }

uint8_t get_al(void) { return cpu.eax & 0xFF; }
uint8_t get_ah(void) { return (cpu.eax >> 8) & 0xFF; }

void set_al(uint8_t v) { cpu.eax = (cpu.eax & 0xFFFFFF00) | v; }
void set_ah(uint8_t v) { cpu.eax = (cpu.eax & 0xFFFF00FF) | (v << 8); }


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
void cpu_set_sp(uint16_t v) {
    set_sp(v);
}
void cpu_set_ah(uint8_t v)
{
    cpu.eax = (cpu.eax & 0xFFFF00FF) | (v << 8);
}

void cpu_set_al(uint8_t v)
{
    cpu.eax = (cpu.eax & 0xFFFFFF00) | v;
}


// ======================================================
//  FETCH / PEEK
// ======================================================

static inline uint8_t fetch8(void)
{
    uint8_t v = mem_read8((cpu.cs << 4) + get_ip());
    cpu.eip++;
    return v;
}
static inline uint16_t fetch16(void)
{
    uint32_t addr = (cpu.cs << 4) + get_ip();
    uint16_t v = mem_read16(addr);
    cpu.eip += 2;
    return v;
}

static inline uint32_t segaddr(uint16_t default_seg, uint16_t offset)
{
    uint16_t seg = cpu.segment_override ? cpu.override_seg : default_seg;
    cpu.segment_override = 0;
    return ((uint32_t)seg << 4) + offset;
}


uint8_t cpu_peek_opcode(void)
{
    uint32_t addr = (cpu.cs << 4) + get_ip();
    return mem_read8(addr);
}
void cpu_interrupt(uint8_t intnum)
{
    handle_int(intnum);
}


// ======================================================
//  FLAGS
// ======================================================


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
void set_sub_flags8(uint8_t dst, uint8_t src, uint8_t result) {
    // CF = borrow
    if (dst < src)
        cpu.eflags |= FLAG_CF;
    else
        cpu.eflags &= ~FLAG_CF;

    // ZF
    if (result == 0)
        cpu.eflags |= FLAG_ZF;
    else
        cpu.eflags &= ~FLAG_ZF;

    // SF
    if (result & 0x80)
        cpu.eflags |= FLAG_SF;
    else
        cpu.eflags &= ~FLAG_SF;

    // PF
    cpu.eflags = (cpu.eflags & ~FLAG_PF) |
                 (((__builtin_popcount(result) & 1) == 0) ? FLAG_PF : 0);

    // OF = signed overflow: (dst ^ src) & (dst ^ result) & 0x80
    if (((dst ^ src) & (dst ^ result) & 0x80) != 0)
        cpu.eflags |= FLAG_OF;
    else
        cpu.eflags &= ~FLAG_OF;
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
    cpu.segment_override = 0;
    cpu.override_seg = 0;

}

// ======================================================
// BOOT SECTOR
// ======================================================
typedef struct {
    uint8_t mod;
    uint8_t reg;
    uint8_t rm;
} ModRM;

static inline ModRM decode_modrm(uint8_t v)
{
    ModRM m;
    m.mod = (v >> 6) & 3;
    m.reg = (v >> 3) & 7;
    m.rm  = (v >> 0) & 7;
    return m;
}

static uint32_t ea16(ModRM m)
{
    switch (m.mod) {

    case 0: // no displacement
        switch (m.rm) {
            case 0: return (get_bx() + get_si()) & 0xFFFF;
            case 1: return (get_bx() + get_di()) & 0xFFFF;
            case 2: return (get_bp() + get_si()) & 0xFFFF;
            case 3: return (get_bp() + get_di()) & 0xFFFF;
            case 4: return get_si();
            case 5: return get_di();
            case 6: {
                uint16_t addr = fetch16();   // ya avanza IP
                return addr;
            }
            case 7: return get_bx();
        }
        break;

    case 1: { // disp8
        int8_t d = (int8_t)fetch8();  // ya avanza IP

        switch (m.rm) {
            case 0: return (get_bx() + get_si() + d) & 0xFFFF;
            case 1: return (get_bx() + get_di() + d) & 0xFFFF;
            case 2: return (get_bp() + get_si() + d) & 0xFFFF;
            case 3: return (get_bp() + get_di() + d) & 0xFFFF;
            case 4: return (get_si() + d) & 0xFFFF;
            case 5: return (get_di() + d) & 0xFFFF;
            case 6: return (get_bp() + d) & 0xFFFF;
            case 7: return (get_bx() + d) & 0xFFFF;
        }
    }

    case 2: { // disp16
        uint16_t d = fetch16();      // ya avanza IP

        switch (m.rm) {
            case 0: return (get_bx() + get_si() + d) & 0xFFFF;
            case 1: return (get_bx() + get_di() + d) & 0xFFFF;
            case 2: return (get_bp() + get_si() + d) & 0xFFFF;
            case 3: return (get_bp() + get_di() + d) & 0xFFFF;
            case 4: return (get_si() + d) & 0xFFFF;
            case 5: return (get_di() + d) & 0xFFFF;
            case 6: return (get_bp() + d) & 0xFFFF;
            case 7: return (get_bx() + d) & 0xFFFF;
        }
    }

    case 3: // register direct
        return 0xFFFFFFFF; // handled elsewhere
    }

    return 0;
}
static inline uint8_t get_reg8(uint8_t r)
{
    switch (r) {
        case 0: return cpu.eax & 0xFF;         // AL
        case 1: return cpu.ecx & 0xFF;         // CL
        case 2: return cpu.edx & 0xFF;         // DL
        case 3: return cpu.ebx & 0xFF;         // BL
        case 4: return (cpu.eax >> 8) & 0xFF;  // AH
        case 5: return (cpu.ecx >> 8) & 0xFF;  // CH
        case 6: return (cpu.edx >> 8) & 0xFF;  // DH
        case 7: return (cpu.ebx >> 8) & 0xFF;  // BH
    }
    return 0;
}

static inline void set_reg8(uint8_t r, uint8_t v)
{
    switch (r) {
        case 0: cpu.eax = (cpu.eax & 0xFFFFFF00) | v; break;
        case 1: cpu.ecx = (cpu.ecx & 0xFFFFFF00) | v; break;
        case 2: cpu.edx = (cpu.edx & 0xFFFFFF00) | v; break;
        case 3: cpu.ebx = (cpu.ebx & 0xFFFFFF00) | v; break;
        case 4: cpu.eax = (cpu.eax & 0xFFFF00FF) | (v << 8); break;
        case 5: cpu.ecx = (cpu.ecx & 0xFFFF00FF) | (v << 8); break;
        case 6: cpu.edx = (cpu.edx & 0xFFFF00FF) | (v << 8); break;
        case 7: cpu.ebx = (cpu.ebx & 0xFFFF00FF) | (v << 8); break;
    }
}
static inline uint16_t get_reg16(uint8_t r)
{
    switch (r) {
        case 0: return get_ax();
        case 1: return get_cx();
        case 2: return get_dx();
        case 3: return get_bx();
        case 4: return get_sp();
        case 5: return get_bp();
        case 6: return get_si();
        case 7: return get_di();
    }
    return 0;
}

static inline void set_reg16(uint8_t r, uint16_t v)
{
    switch (r) {
        case 0: set_ax(v); break;
        case 1: set_cx(v); break;
        case 2: set_dx(v); break;
        case 3: set_bx(v); break;
        case 4: set_sp(v); break;
        case 5: set_bp(v); break;
        case 6: set_si(v); break;
        case 7: set_di(v); break;
    }
}
static inline uint16_t default_seg16(ModRM m)
{
    if (m.mod != 3) {
        switch (m.rm) {
            case 2: // [BP+SI]
            case 3: // [BP+DI]
            case 6: // [BP] o [BP+disp]
                return cpu.ss;   // ESTO ES LO QUE TE FALTA
        }
    }
    return cpu.ds;
}

void cpu_iret(void)
{
    uint16_t sp = cpu_get_sp();

    uint16_t ip = mem_read16((cpu.ss << 4) + sp);
    sp += 2;

    uint16_t cs = mem_read16((cpu.ss << 4) + sp);
    sp += 2;

    uint16_t flags = mem_read16((cpu.ss << 4) + sp);
    sp += 2;

    cpu_set_sp(sp);
    cpu.cs = cs;
    cpu.eflags = (cpu.eflags & 0xFFFF0000) | flags;
    cpu_set_ip(ip);
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
    case 0xFE: {   // FE /r  INC/DEC r/m8
    uint8_t modrm = fetch8();
    ModRM m = decode_modrm(modrm);

    uint8_t val;
    uint32_t ea, addr;

    if (m.mod == 3) {
        val = get_reg8(m.rm);
    } else {
        ea   = ea16(m);
        addr = segaddr(cpu.ds, ea);
        val  = mem_read8(addr);
    }

    uint8_t result;

    switch (m.reg) {

        case 0: // INC r/m8
            result = val + 1;

            // FLAGS: OF, SF, ZF, PF (CF no cambia)
            if (val == 0x7F) cpu.eflags |= FLAG_OF;
            else cpu.eflags &= ~FLAG_OF;

            if (result == 0) cpu.eflags |= FLAG_ZF; else cpu.eflags &= ~FLAG_ZF;
            if (result & 0x80) cpu.eflags |= FLAG_SF; else cpu.eflags &= ~FLAG_SF;

            cpu.eflags = (cpu.eflags & ~FLAG_PF) |
                         (((__builtin_popcount(result) & 1) == 0) ? FLAG_PF : 0);

            break;

        case 1: // DEC r/m8
            result = val - 1;

            // FLAGS: OF, SF, ZF, PF (CF no cambia)
            if (val == 0x80) cpu.eflags |= FLAG_OF;
            else cpu.eflags &= ~FLAG_OF;

            if (result == 0) cpu.eflags |= FLAG_ZF; else cpu.eflags &= ~FLAG_ZF;
            if (result & 0x80) cpu.eflags |= FLAG_SF; else cpu.eflags &= ~FLAG_SF;

            cpu.eflags = (cpu.eflags & ~FLAG_PF) |
                         (((__builtin_popcount(result) & 1) == 0) ? FLAG_PF : 0);

            break;

        default:
            Print(L"[CPU] FE /%d no implementado\n", m.reg);
            return;
    }

    // write back
    if (m.mod == 3)
        set_reg8(m.rm, result);
    else
        mem_write8(addr, result);

    return;
}
case 0x08: {   // OR r/m8, r8
    uint8_t modrm = fetch8();
    ModRM m = decode_modrm(modrm);

    uint8_t src = get_reg8(m.reg);  // r8
    uint8_t dst;
    uint32_t ea, addr;

    if (m.mod == 3) {
        dst = get_reg8(m.rm);
    } else {
        ea   = ea16(m);
        addr = segaddr(cpu.ds, ea);
        dst  = mem_read8(addr);
    }

    uint8_t result = dst | src;

    // write back
    if (m.mod == 3)
        set_reg8(m.rm, result);
    else
        mem_write8(addr, result);

    // FLAGS: CF=0, OF=0, ZF/SF/PF actualizados
    cpu.eflags &= ~(FLAG_CF | FLAG_OF);

    if (result == 0) cpu.eflags |= FLAG_ZF; else cpu.eflags &= ~FLAG_ZF;
    if (result & 0x80) cpu.eflags |= FLAG_SF; else cpu.eflags &= ~FLAG_SF;

    cpu.eflags = (cpu.eflags & ~FLAG_PF) |
                 (((__builtin_popcount(result) & 1) == 0) ? FLAG_PF : 0);

    return;
}

    case 0x03: {   // ADD r16, r/m16
    uint8_t modrm_byte = fetch8();
    ModRM m = decode_modrm(modrm_byte);

    uint16_t src;
    if (m.mod == 3) {
        // ADD reg16, reg16
        src = get_reg16(m.rm);
    } else {
        uint32_t ea   = ea16(m);
        uint16_t seg = cpu.segment_override ? cpu.override_seg : cpu.ds;
        cpu.segment_override = 0;   // se consume el override
        uint32_t addr = (seg << 4) + ea;
        src = mem_read16(addr);
    }

    uint16_t dst  = get_reg16(m.reg);
    uint32_t r32  = (uint32_t)dst + src;

    set_reg16(m.reg, (uint16_t)r32);
    set_add_flags16(dst, src, r32);
    return;
    }


    case 0xC7: {   // C7 /0  MOV r/m16, imm16
    uint8_t modrm_byte = fetch8();
    ModRM m = decode_modrm(modrm_byte);

    if (m.reg != 0) {
        Print(L"[CPU] C7 /%d no implementado (solo /0 MOV)\n", m.reg);
        return;
    }

    uint16_t imm = fetch8();
    imm |= ((uint16_t)fetch8()) << 8;

    if (m.mod == 3) {
        // MOV reg16, imm16
        set_reg16(m.rm, imm);
        return;
    }

    uint32_t ea   = ea16(m);
    uint16_t seg = cpu.segment_override ? cpu.override_seg : cpu.ds;
    cpu.segment_override = 0;   // se consume el override
    uint32_t addr = (seg << 4) + ea;
    mem_write16(addr, imm);
    return;
}

case 0x8B: {   // MOV r16, r/m16
    uint8_t modrm_byte = fetch8();
    ModRM m = decode_modrm(modrm_byte);

    uint16_t val;
    if (m.mod == 3) {
        val = get_reg16(m.rm);
    } else {
        uint32_t ea   = ea16(m);
        uint16_t seg = cpu.segment_override ? cpu.override_seg : cpu.ds;
        cpu.segment_override = 0;   // se consume el override
        uint32_t addr = (seg << 4) + ea;    
        val = mem_read16(addr);
    }

    set_reg16(m.reg, val);
    return;
}
case 0xA1: {   // MOV AX, [imm16]
    uint16_t off = fetch8();
    off |= ((uint16_t)fetch8()) << 8;

    uint32_t addr = (cpu.ds << 4) + off;
    uint16_t val  = mem_read16(addr);

    set_ax(val);
    return;
}
case 0xA3: {   // MOV [imm16], AX
    uint16_t off = fetch8();
    off |= ((uint16_t)fetch8()) << 8;

    uint32_t addr = (cpu.ds << 4) + off;
    uint16_t ax   = get_ax();

    mem_write16(addr, ax);
    return;
}
case 0xEA: {   // JMP FAR ptr16:16
    uint16_t ip = fetch8() | (fetch8() << 8);
    uint16_t cs = fetch8() | (fetch8() << 8);
    cpu.cs = cs;
    set_ip(ip);
    return;
}

case 0xE8: {   // CALL rel16
    int16_t rel = fetch8();
    rel |= ((int16_t)fetch8()) << 8;

    uint16_t return_ip = get_ip();  // IP después del fetch

    // push return address
    uint16_t sp = get_sp() - 2;
    set_sp(sp);
    mem_write16((cpu.ss << 4) + sp, return_ip);

    // salto relativo
    set_ip(return_ip + rel);
    return;
}
case 0xAC: {   // LODSB
    uint32_t src = segaddr(cpu.ds, get_si());
    uint8_t v = mem_read8(src);
    cpu.eax = (cpu.eax & 0xFFFFFF00) | v;  // AL = v

    if (cpu.eflags & FLAG_DF)
        set_si(get_si() - 1);
    else
        set_si(get_si() + 1);

    return;
}
case 0x56: {   // PUSH SI
    uint16_t sp = get_sp() - 2;
    set_sp(sp);
    mem_write16((cpu.ss << 4) + sp, get_si());
    return;
}
case 0x5E: {   // POP SI
    uint16_t sp  = get_sp();
    uint16_t val = mem_read16((cpu.ss << 4) + sp);
    set_sp(sp + 2);
    set_si(val);
    return;
}
case 0xC3: {   // RET
    uint16_t sp = get_sp();
    uint16_t ip = mem_read16((cpu.ss << 4) + sp);
    set_sp(sp + 2);
    set_ip(ip);
    return;
}
case 0x01: {   // ADD r/m16, r16
    uint8_t modrm = fetch8();
    ModRM m = decode_modrm(modrm);

    uint16_t src = get_reg16(m.reg);

    if (m.mod == 3) {
        uint16_t dst = get_reg16(m.rm);
        uint32_t r32 = (uint32_t)dst + src;
        set_reg16(m.rm, (uint16_t)r32);
        set_add_flags16(dst, src, r32);
        return;
    }

    uint32_t ea   = ea16(m);
    uint16_t seg = cpu.segment_override ? cpu.override_seg : cpu.ds;
    cpu.segment_override = 0;   // se consume el override
    uint32_t addr = (seg << 4) + ea;

    uint16_t dst = mem_read16(addr);
    uint32_t r32 = (uint32_t)dst + src;

    mem_write16(addr, (uint16_t)r32);
    set_add_flags16(dst, src, r32);
    return;
}
case 0x8A: {   // MOV r8, r/m8
    uint8_t modrm = fetch8();
    ModRM m = decode_modrm(modrm);

    uint8_t val;

    if (m.mod == 3) {
        // MOV r8, r8
        val = get_reg8(m.rm);
    } else {
        uint32_t ea = ea16(m);
        uint32_t addr = segaddr(cpu.ds, ea);   // respeta overrides
        val = mem_read8(addr);
    }

    set_reg8(m.reg, val);
    return;
}
case 0x0F: {
    uint8_t op2 = fetch8();

    switch (op2) {

    case 0xB6: {   // MOVZX r16, r/m8
        uint8_t modrm = fetch8();
        ModRM m = decode_modrm(modrm);

        uint8_t val;

        if (m.mod == 3) {
            val = get_reg8(m.rm);
        } else {
            uint32_t ea = ea16(m);
            uint32_t addr = segaddr(cpu.ds, ea);
            val = mem_read8(addr);
        }

        set_reg16(m.reg, (uint16_t)val);
        return;
    }

    default:
        Print(L"[CPU] 0F %02x no implementado\n", op2);
        return;
    }
}
case 0xA0: {   // MOV AL, [imm16]
    uint16_t off = fetch8();
    off |= ((uint16_t)fetch8()) << 8;

    uint32_t addr = segaddr(cpu.ds, off);
    uint8_t val = mem_read8(addr);

    cpu.eax = (cpu.eax & 0xFFFFFF00) | val;
    return;
}
case 0xA2: {   // MOV [imm16], AL
    uint16_t off = fetch8();
    off |= ((uint16_t)fetch8()) << 8;

    uint32_t addr = segaddr(cpu.ds, off);
    uint8_t al = cpu.eax & 0xFF;

    mem_write8(addr, al);
    return;
}
case 0xC6: {   // C6 /0  MOV r/m8, imm8
    uint8_t modrm = fetch8();
    ModRM m = decode_modrm(modrm);

    if (m.reg != 0) {
        Print(L"[CPU] C6 /%d no implementado\n", m.reg);
        return;
    }

    uint8_t imm = fetch8();

    if (m.mod == 3) {
        set_reg8(m.rm, imm);
        return;
    }

    uint32_t ea = ea16(m);
    uint32_t addr = segaddr(cpu.ds, ea);
    mem_write8(addr, imm);
    return;
}
case 0x9A: {   // CALL FAR ptr16:16
    uint16_t ip = fetch8() | (fetch8() << 8);
    uint16_t cs = fetch8() | (fetch8() << 8);

    uint16_t sp = get_sp() - 2;
    set_sp(sp);
    mem_write16((cpu.ss << 4) + sp, get_ip());

    sp = get_sp() - 2;
    set_sp(sp);
    mem_write16((cpu.ss << 4) + sp, cpu.cs);

    cpu.cs = cs;
    set_ip(ip);
    return;
}
case 0xCB: {   // RETF
    uint16_t sp = get_sp();
    uint16_t ip = mem_read16((cpu.ss << 4) + sp);
    sp += 2;

    uint16_t cs = mem_read16((cpu.ss << 4) + sp);
    sp += 2;

    set_sp(sp);
    cpu.cs = cs;
    set_ip(ip);
    return;
}
case 0x98: {   // CBW - Convert Byte to Word
    int8_t al = cpu.eax & 0xFF;
    int16_t ax = (int16_t)al;
    set_ax((uint16_t)ax);
    return;
}
case 0xF7: {   // F7 /x r/m16
    uint8_t modrm = fetch8();
    ModRM m = decode_modrm(modrm);

    uint32_t ea, addr;
    uint16_t val;

    if (m.mod == 3) {
        val = get_reg16(m.rm);
    } else {
        ea = ea16(m);
        uint16_t seg = default_seg16(m);
        addr = segaddr(seg, ea);
        val = mem_read16(addr);
    }

    switch (m.reg) {

        case 2: { // NOT r/m16
            uint16_t r = ~val;
            if (m.mod == 3) set_reg16(m.rm, r);
            else mem_write16(addr, r);
            return;
        }

        case 3: { // NEG r/m16
            uint16_t r = (uint16_t)(0 - val);

            // CF = 1 si val != 0
            if (val != 0) cpu.eflags |= FLAG_CF;
            else cpu.eflags &= ~FLAG_CF;

            // OF = 1 si val == 0x8000
            if (val == 0x8000) cpu.eflags |= FLAG_OF;
            else cpu.eflags &= ~FLAG_OF;

            // ZF, SF
            if (r == 0) cpu.eflags |= FLAG_ZF; else cpu.eflags &= ~FLAG_ZF;
            if (r & 0x8000) cpu.eflags |= FLAG_SF; else cpu.eflags &= ~FLAG_SF;

            if (m.mod == 3) set_reg16(m.rm, r);
            else mem_write16(addr, r);
            return;
        }

        case 4: { // MUL r/m16  (AX * r/m16 = DX:AX)
            uint32_t ax = get_ax();
            uint32_t res = ax * val;

            set_ax((uint16_t)(res & 0xFFFF));
            set_dx((uint16_t)(res >> 16));

            // CF = OF = 1 si el resultado no cabe en 16 bits
            if (res > 0xFFFF) cpu.eflags |= FLAG_CF | FLAG_OF;
            else cpu.eflags &= ~(FLAG_CF | FLAG_OF);

            return;
        }

        case 5: { // IMUL r/m16  (signed)
            int32_t ax = (int16_t)get_ax();
            int32_t v  = (int16_t)val;
            int32_t res = ax * v;

            set_ax((uint16_t)(res & 0xFFFF));
            set_dx((uint16_t)(res >> 16));

            // CF = OF = 1 si no cabe en 16 bits sign-extended
            int32_t signext = (int16_t)(res & 0xFFFF);
            if (res != signext) cpu.eflags |= FLAG_CF | FLAG_OF;
            else cpu.eflags &= ~(FLAG_CF | FLAG_OF);

            return;
        }

        case 6: { // DIV r/m16
    uint32_t dividend = ((uint32_t)get_dx() << 16) | get_ax();

    // División por cero → INT 0
    if (val == 0) {
        cpu_interrupt(0);
        return;
    }

    uint32_t q = dividend / val;
    uint32_t r = dividend % val;

    // Si el cociente no cabe en AX → INT 0
    if (q > 0xFFFF) {
        cpu_interrupt(0);
        return;
    }

    set_ax((uint16_t)q);
    set_dx((uint16_t)r);
    return;
    }




case 7: { // IDIV r/m16
    int32_t dividend = ((int32_t)(int16_t)get_dx() << 16) | (uint16_t)get_ax();
    int16_t divisor  = (int16_t)val;

    if (divisor == 0) {
        cpu_interrupt(0);
        return;
    }

    int32_t q = dividend / divisor;
    int32_t r = dividend % divisor;

    // Si el cociente no cabe en AX (signed 16-bit) → INT 0
    if (q < -32768 || q > 32767) {
        cpu_interrupt(0);
        return;
    }

    set_ax((uint16_t)q);
    set_dx((uint16_t)r);
    return;
}


        default:
            Print(L"[CPU] F7 /%d no implementado\n", m.reg);
            return;
    }
}
case 0x11: {   // ADC r/m16, r16
    uint8_t modrm = fetch8();
    ModRM m = decode_modrm(modrm);

    uint16_t src = get_reg16(m.reg);   // r16
    uint16_t dst;
    uint32_t ea, addr;

    if (m.mod == 3) {
        dst = get_reg16(m.rm);
    } else {
        ea   = ea16(m);
        addr = segaddr(cpu.ds, ea);
        dst  = mem_read16(addr);
    }

    uint32_t carry = (cpu.eflags & FLAG_CF) ? 1 : 0;
    uint32_t r32 = (uint32_t)dst + src + carry;

    if (m.mod == 3)
        set_reg16(m.rm, (uint16_t)r32);
    else
        mem_write16(addr, (uint16_t)r32);

    set_add_flags16(dst, src + carry, r32);
    return;
}
case 0xC4: {   // LES r16, r/m16
    uint8_t modrm = fetch8();
    ModRM m = decode_modrm(modrm);

    uint32_t ea, addr;

    if (m.mod == 3) {
        Print(L"[CPU] LES con mod=3 no válido\n");
        return;
    }

    ea   = ea16(m);
    addr = segaddr(cpu.ds, ea);

    uint16_t offset = mem_read16(addr);
    uint16_t seg    = mem_read16(addr + 2);

    set_reg16(m.reg, offset);
    cpu.es = seg;

    return;
}
case 0x8C: {   // MOV r/m16, Sreg
    uint8_t modrm = fetch8();
    ModRM m = decode_modrm(modrm);

    uint16_t segval;

    switch (m.reg) {
        case 0: segval = cpu.es; break;
        case 1: segval = cpu.cs; break;
        case 2: segval = cpu.ss; break;
        case 3: segval = cpu.ds; break;
        default:
            Print(L"[CPU] 8C con Sreg=%d no implementado\n", m.reg);
            return;
    }

    if (m.mod == 3) {
        // MOV reg16, Sreg
        set_reg16(m.rm, segval);
        return;
    }

    uint32_t ea   = ea16(m);
    uint32_t addr = segaddr(cpu.ds, ea);

    mem_write16(addr, segval);
    return;
}
case 0x84: {   // TEST r/m8, r8
    uint8_t modrm = fetch8();
    ModRM m = decode_modrm(modrm);

    uint8_t src = get_reg8(m.reg);
    uint8_t dst;
    uint32_t ea, addr;

    if (m.mod == 3) {
        dst = get_reg8(m.rm);
    } else {
        ea   = ea16(m);
        addr = segaddr(cpu.ds, ea);
        dst  = mem_read8(addr);
    }

    uint8_t r = dst & src;

    // FLAGS: ZF, SF, PF; CF=0, OF=0
    if (r == 0) cpu.eflags |= FLAG_ZF; else cpu.eflags &= ~FLAG_ZF;
    if (r & 0x80) cpu.eflags |= FLAG_SF; else cpu.eflags &= ~FLAG_SF;

    cpu.eflags &= ~FLAG_CF;
    cpu.eflags &= ~FLAG_OF;

    // PF (parity)
    cpu.eflags = (cpu.eflags & ~FLAG_PF) |
                 (((__builtin_popcount(r) & 1) == 0) ? FLAG_PF : 0);

    return;
}
case 0x33: {   // XOR r16, r/m16
    uint8_t modrm = fetch8();
    ModRM m = decode_modrm(modrm);

    uint16_t src;
    if (m.mod == 3)
        src = get_reg16(m.rm);
    else
        src = mem_read16(segaddr(cpu.ds, ea16(m)));

    uint16_t dst = get_reg16(m.reg);
    uint16_t result = dst ^ src;

    set_reg16(m.reg, result);

    cpu.eflags &= ~(FLAG_CF | FLAG_OF | FLAG_SF | FLAG_ZF);
    if (result == 0) cpu.eflags |= FLAG_ZF;
    if (result & 0x8000) cpu.eflags |= FLAG_SF;

    return;
}
case 0x32: {   // XOR r8, r/m8
    uint8_t modrm = fetch8();
    ModRM m = decode_modrm(modrm);

    uint8_t src;
    if (m.mod == 3)
        src = get_reg8(m.rm);
    else
        src = mem_read8(segaddr(cpu.ds, ea16(m)));

    uint8_t dst = get_reg8(m.reg);
    uint8_t result = dst ^ src;

    set_reg8(m.reg, result);

    cpu.eflags &= ~(FLAG_CF | FLAG_OF | FLAG_SF | FLAG_ZF);
    if (result == 0) cpu.eflags |= FLAG_ZF;
    if (result & 0x80) cpu.eflags |= FLAG_SF;

    return;
}

case 0xF6: {   // F6 /x r/m8
    uint8_t modrm = fetch8();
    ModRM m = decode_modrm(modrm);

    uint32_t ea, addr;
    uint8_t val;

    if (m.mod == 3) {
        // r/m8 = registro de 8 bits
        val = get_reg8(m.rm);
    } else {
        ea = ea16(m);
        uint16_t seg = default_seg16(m);
        addr = segaddr(seg, ea);
        val = mem_read8(addr);
    }

    switch (m.reg) {

        case 0: { // TEST r/m8, imm8
            uint8_t imm = fetch8();
            uint8_t r = val & imm;

            // FLAGS: ZF, SF, PF; CF=0, OF=0
            if (r == 0) cpu.eflags |= FLAG_ZF; else cpu.eflags &= ~FLAG_ZF;
            if (r & 0x80) cpu.eflags |= FLAG_SF; else cpu.eflags &= ~FLAG_SF;

            cpu.eflags &= ~FLAG_CF;
            cpu.eflags &= ~FLAG_OF;

            cpu.eflags = (cpu.eflags & ~FLAG_PF) |
                         (((__builtin_popcount(r) & 1) == 0) ? FLAG_PF : 0);

            return;
        }

        case 2: { // NOT r/m8
            uint8_t r = ~val;

            if (m.mod == 3) set_reg8(m.rm, r);
            else mem_write8(addr, r);

            return;
        }

        case 3: { // NEG r/m8
            uint8_t r = (uint8_t)(0 - val);

            // CF = 1 si val != 0
            if (val != 0) cpu.eflags |= FLAG_CF;
            else cpu.eflags &= ~FLAG_CF;

            // OF = 1 si val == 0x80
            if (val == 0x80) cpu.eflags |= FLAG_OF;
            else cpu.eflags &= ~FLAG_OF;

            // ZF, SF
            if (r == 0) cpu.eflags |= FLAG_ZF; else cpu.eflags &= ~FLAG_ZF;
            if (r & 0x80) cpu.eflags |= FLAG_SF; else cpu.eflags &= ~FLAG_SF;

            if (m.mod == 3) set_reg8(m.rm, r);
            else mem_write8(addr, r);

            return;
        }

        case 4: { // MUL r/m8  (AL * r/m8 = AX)
            uint16_t al = cpu.eax & 0xFF;
            uint16_t res = al * val;

            set_ax(res);

            // CF = OF = 1 si el resultado no cabe en 8 bits
            if (res > 0xFF) cpu.eflags |= FLAG_CF | FLAG_OF;
            else cpu.eflags &= ~(FLAG_CF | FLAG_OF);

            return;
        }

        case 5: { // IMUL r/m8  (signed)
            int16_t al = (int8_t)(cpu.eax & 0xFF);
            int16_t v  = (int8_t)val;
            int16_t res = al * v;

            set_ax((uint16_t)res);

            // CF = OF = 1 si no cabe en 8 bits sign-extended
            int16_t signext = (int8_t)(res & 0xFF);
            if (res != signext) cpu.eflags |= FLAG_CF | FLAG_OF;
            else cpu.eflags &= ~(FLAG_CF | FLAG_OF);

            return;
        }

        case 6: { // DIV r/m8
            uint16_t ax = get_ax();

            if (val == 0 || (ax / val) > 0xFF) {
            cpu_interrupt(0);   // INT 0 - Divide Error
            return;
            }


            uint8_t quotient  = (uint8_t)(ax / val);
            uint8_t remainder = (uint8_t)(ax % val);

            set_al(quotient);
            set_ah(remainder);
            return;
        }

        case 7: { // IDIV r/m8
    int16_t ax = (int16_t)get_ax();
    int8_t divisor = (int8_t)val;

    if (divisor == 0) {
        cpu_interrupt(0);
        return;
    }

    int16_t q = ax / divisor;
    int16_t r = ax % divisor;

    if (q < -128 || q > 127) {
        cpu_interrupt(0);
        return;
    }

    set_al((uint8_t)q);
    set_ah((uint8_t)r);
    return;
    }


        default:
            Print(L"[CPU] F6 /%d no implementado\n", m.reg);
            return;
    }
}


case 0x83: {   // 83 /x r/m16, imm8
    uint8_t modrm = fetch8();
    ModRM m = decode_modrm(modrm);

    int8_t imm = (int8_t)fetch8();

    uint16_t dst;
    uint32_t ea, addr;

    if (m.mod == 3) {
        dst = get_reg16(m.rm);
    } else {
        ea   = ea16(m);
        addr = addr = segaddr(cpu.ds, ea);
        dst  = mem_read16(addr);
    }

    uint32_t r32;

    switch (m.reg) {
        case 0: // ADD
            r32 = (uint32_t)dst + imm;
            if (m.mod == 3) set_reg16(m.rm, (uint16_t)r32);
            else mem_write16(addr, (uint16_t)r32);
            set_add_flags16(dst, imm, r32);
            return;
        case 2: { // ADC r/m16, imm8
            uint32_t carry = (cpu.eflags & FLAG_CF) ? 1 : 0;
            r32 = (uint32_t)dst + imm + carry;


            if (m.mod == 3) set_reg16(m.rm, (uint16_t)r32);
            else mem_write16(addr, (uint16_t)r32);

            set_add_flags16(dst, imm + carry, r32);
            return;
            }


        case 5: // SUB
            r32 = (uint32_t)dst - imm;
            if (m.mod == 3) set_reg16(m.rm, (uint16_t)r32);
            else mem_write16(addr, (uint16_t)r32);
            set_sub_flags16(dst, imm, r32);
            return;

        case 7: // CMP
            r32 = (uint32_t)dst - imm;
            set_sub_flags16(dst, imm, r32);
            return;

        default:
            Print(L"[CPU] 83 /%d no implementado\n", m.reg);
            return;
    }
}
case 0xD1: {   // D1 /x r/m16,1
    uint8_t modrm = fetch8();
    ModRM m = decode_modrm(modrm);

    uint16_t val;
    uint32_t ea, addr;

    if (m.mod == 3) val = get_reg16(m.rm);
    else {
        ea   = ea16(m);
        addr = (cpu.ds << 4) + ea;
        val  = mem_read16(addr);
    }

    switch (m.reg) {
        case 4: // SHL
            val <<= 1;
            break;
        case 5: // SHR
            val >>= 1;
            break;
        case 7: // SAR
            val = (int16_t)val >> 1;
            break;
        default:
            Print(L"[CPU] D1 /%d no implementado\n", m.reg);
            return;
    }

    if (m.mod == 3) set_reg16(m.rm, val);
    else mem_write16(addr, val);

    return;
}
case 0xD3: {   // D3 /x r/m16,CL
    uint8_t modrm = fetch8();
    ModRM m = decode_modrm(modrm);

    uint16_t val;
    uint32_t ea, addr;

    if (m.mod == 3) val = get_reg16(m.rm);
    else {
        ea   = ea16(m);
        addr = (cpu.ds << 4) + ea;
        val  = mem_read16(addr);
    }

    uint8_t count = get_cx() & 0xFF;

    switch (m.reg) {
        case 4: val <<= count; break; // SHL
        case 5: val >>= count; break; // SHR
        case 7: val = (int16_t)val >> count; break; // SAR
        default:
            Print(L"[CPU] D3 /%d no implementado\n", m.reg);
            return;
    }

    if (m.mod == 3) set_reg16(m.rm, val);
    else mem_write16(addr, val);

    return;
}
case 0x26: {   // ES: override
    cpu.segment_override = 1;
    cpu.override_seg = cpu.es;
    return;
}
case 0xE9: {   // JMP rel16
    int16_t rel = fetch8();
    rel |= ((int16_t)fetch8()) << 8;
    set_ip(get_ip() + rel);
    return;
}



   case 0x90: return; // NOP

case 0x91: { // XCHG CX, AX
    uint16_t ax = get_ax(), cx = get_cx();
    set_ax(cx); set_cx(ax);
    return;
}

case 0x92: { // XCHG DX, AX
    uint16_t ax = get_ax(), dx = get_dx();
    set_ax(dx); set_dx(ax);
    return;
}

case 0x93: { // XCHG BX, AX
    uint16_t ax = get_ax(), bx = get_bx();
    set_ax(bx); set_bx(ax);
    return;
}

case 0x94: { // XCHG SP, AX
    uint16_t ax = get_ax(), sp = get_sp();
    set_ax(sp); set_sp(ax);
    return;
}

case 0x95: { // XCHG BP, AX
    uint16_t ax = get_ax(), bp = get_bp();
    set_ax(bp); set_bp(ax);
    return;
}

case 0x96: { // XCHG SI, AX
    uint16_t ax = get_ax(), si = get_si();
    set_ax(si); set_si(ax);
    return;
}

case 0x97: { // XCHG DI, AX
    uint16_t ax = get_ax(), di = get_di();
    set_ax(di); set_di(ax);
    return;
}
case 0x88: {   // MOV r/m8, r8
    uint8_t modrm = fetch8();
    ModRM m = decode_modrm(modrm);

    uint8_t src = get_reg8(m.reg);  // r8

    if (m.mod == 3) {
        // MOV r8, r8
        set_reg8(m.rm, src);
        return;
    }

    uint32_t ea   = ea16(m);
    uint32_t addr = segaddr(cpu.ds, ea);

    mem_write8(addr, src);
    return;
}
case 0x86: {   // XCHG r/m8, r8
    uint8_t modrm = fetch8();
    ModRM m = decode_modrm(modrm);

    uint8_t src = get_reg8(m.reg);  // r8
    uint8_t dst;

    if (m.mod == 3) {
        // XCHG reg8, reg8
        dst = get_reg8(m.rm);
        set_reg8(m.rm, src);
        set_reg8(m.reg, dst);
        return;
    }

    uint32_t ea   = ea16(m);
    uint32_t addr = segaddr(cpu.ds, ea);

    dst = mem_read8(addr);

    // swap
    mem_write8(addr, src);
    set_reg8(m.reg, dst);

    return;
}
case 0xD0: {   // D0 /r  SHIFT/ROTATE r/m8, 1
    uint8_t modrm = fetch8();
    ModRM m = decode_modrm(modrm);

    uint8_t val;
    uint32_t ea, addr;

    if (m.mod == 3) {
        val = get_reg8(m.rm);
    } else {
        ea   = ea16(m);
        addr = segaddr(cpu.ds, ea);
        val  = mem_read8(addr);
    }

    uint8_t result;
    uint8_t carry;

    switch (m.reg) {

        case 0: // ROL r/m8, 1
            carry = (val & 0x80) ? 1 : 0;
            result = (val << 1) | carry;
            cpu.eflags = (cpu.eflags & ~FLAG_CF) | (carry ? FLAG_CF : 0);
            break;

        case 1: // ROR r/m8, 1
            carry = (val & 1);
            result = (val >> 1) | (carry ? 0x80 : 0);
            cpu.eflags = (cpu.eflags & ~FLAG_CF) | (carry ? FLAG_CF : 0);
            break;

        case 2: // RCL r/m8, 1
        {
            uint8_t old_cf = (cpu.eflags & FLAG_CF) ? 1 : 0;
            carry = (val & 0x80) ? 1 : 0;
            result = (val << 1) | old_cf;
            cpu.eflags = (cpu.eflags & ~FLAG_CF) | (carry ? FLAG_CF : 0);
            break;
        }

        case 3: // RCR r/m8, 1
        {
            uint8_t old_cf = (cpu.eflags & FLAG_CF) ? 1 : 0;
            carry = (val & 1);
            result = (val >> 1) | (old_cf ? 0x80 : 0);
            cpu.eflags = (cpu.eflags & ~FLAG_CF) | (carry ? FLAG_CF : 0);
            break;
        }

        case 4: // SHL r/m8, 1
            carry = (val & 0x80) ? 1 : 0;
            result = val << 1;
            cpu.eflags = (cpu.eflags & ~FLAG_CF) | (carry ? FLAG_CF : 0);
            break;

        case 5: // SHR r/m8, 1
            carry = (val & 1);
            result = val >> 1;
            cpu.eflags = (cpu.eflags & ~FLAG_CF) | (carry ? FLAG_CF : 0);
            break;

        case 7: // SAR r/m8, 1
            carry = (val & 1);
            result = (val >> 1) | (val & 0x80); // sign extend
            cpu.eflags = (cpu.eflags & ~FLAG_CF) | (carry ? FLAG_CF : 0);
            break;

        default:
            Print(L"[CPU] D0 /%d no implementado\n", m.reg);
            return;
    }

    // ZF, SF, PF
    if (result == 0) cpu.eflags |= FLAG_ZF; else cpu.eflags &= ~FLAG_ZF;
    if (result & 0x80) cpu.eflags |= FLAG_SF; else cpu.eflags &= ~FLAG_SF;
    cpu.eflags = (cpu.eflags & ~FLAG_PF) |
                 (((__builtin_popcount(result) & 1) == 0) ? FLAG_PF : 0);

    // write back
    if (m.mod == 3)
        set_reg8(m.rm, result);
    else
        mem_write8(addr, result);

    return;
}
case 0x28: {   // SUB r/m8, r8
    uint8_t modrm = fetch8();
    ModRM m = decode_modrm(modrm);

    uint8_t src = get_reg8(m.reg);
    uint8_t dst;
    uint32_t ea, addr;

    if (m.mod == 3) {
        dst = get_reg8(m.rm);
    } else {
        ea   = ea16(m);
        addr = segaddr(cpu.ds, ea);
        dst  = mem_read8(addr);
    }

    uint16_t r16 = (uint16_t)dst - (uint16_t)src;

    uint8_t result = (uint8_t)r16;

    if (m.mod == 3)
        set_reg8(m.rm, result);
    else
        mem_write8(addr, result);

    set_sub_flags8(dst, src, result);
    return;
}
case 0x2A: {   // SUB r8, r/m8
    uint8_t modrm = fetch8();
    ModRM m = decode_modrm(modrm);

    uint8_t src;
    uint8_t dst = get_reg8(m.reg);
    uint32_t ea, addr;

    if (m.mod == 3) {
        src = get_reg8(m.rm);
    } else {
        ea   = ea16(m);
        addr = segaddr(cpu.ds, ea);
        src  = mem_read8(addr);
    }

    uint16_t r16 = (uint16_t)dst - (uint16_t)src;
    uint8_t result = (uint8_t)r16;

    set_reg8(m.reg, result);
    set_sub_flags8(dst, src, result);
    return;
}


        // Interrup Flag
    case 0xFA: {   // CLI
    cpu.eflags &= ~0x0200;   // IF = 0
    return;
    }
    case 0xFB: cpu.eflags |= FLAG_IF; return;
    case 0xFC: cpu.eflags &= ~FLAG_DF; return;
    case 0xFD: cpu.eflags |= FLAG_DF; return;
    case 0x9C: {   // PUSHF
    uint16_t sp = get_sp() - 2;
    set_sp(sp);
    mem_write16((cpu.ss << 4) + sp, (uint16_t)(cpu.eflags & 0xFFFF));
    return;
    }
    case 0x9D: {   // POPF
    uint16_t sp = get_sp();
    uint16_t flags = mem_read16((cpu.ss << 4) + sp);
    set_sp(sp + 2);

    // Solo bits válidos en modo real
    cpu.eflags = (cpu.eflags & 0xFFFF0000) | (flags & 0x0FD7);

    return;
    }

case 0xCF: {   // IRET
    uint16_t sp = get_sp();

    uint16_t ip  = mem_read16((cpu.ss << 4) + sp);
    sp += 2;

    uint16_t cs  = mem_read16((cpu.ss << 4) + sp);
    sp += 2;

    uint16_t flg = mem_read16((cpu.ss << 4) + sp);
    sp += 2;

    set_sp(sp);

    cpu.cs = cs;
    set_ip(ip);

    // Restaurar flags 8086 reales
    cpu.eflags = (cpu.eflags & 0xFFFF0000) | (flg & 0x0FD5);

    return;
}

    // 00

    case 0x00: {   // ADD r/m8, r8
    uint8_t modrm_byte = fetch8();
    ModRM m = decode_modrm(modrm_byte);

    uint8_t src = get_reg8(m.reg);

    if (m.mod == 3) {
        // ADD reg8, reg8
        uint8_t dst = get_reg8(m.rm);
        uint16_t r = dst + src;
        set_reg8(m.rm, (uint8_t)r);
        set_logic_flags16(r);
        return;
    }

    uint32_t ea = ea16(m);
    uint16_t seg = cpu.segment_override ? cpu.override_seg : cpu.ds;
    cpu.segment_override = 0;   // se consume el override
    uint32_t addr = (seg << 4) + ea;
    uint8_t dst = mem_read8(addr);
    uint16_t r = dst + src;

    mem_write8(addr, (uint8_t)r);
    set_logic_flags16(r);

    return;
    }

    case 0x8E: {   // MOV Sreg, r/m16
    uint8_t modrm_byte = fetch8();
    ModRM m = decode_modrm(modrm_byte);

    uint16_t val;

    if (m.mod == 3) {
        val = get_reg16(m.rm);
    } else {
        uint32_t ea = ea16(m);
        uint16_t seg = cpu.segment_override ? cpu.override_seg : cpu.ds;
        cpu.segment_override = 0;   // se consume el override
        uint32_t addr = (seg << 4) + ea;
        val = mem_read16(addr);
    }

    switch (m.reg) {
        case 0: cpu.es = val; break;
        case 1: cpu.cs = val; break;
        case 2: cpu.ss = val; break;
        case 3: cpu.ds = val; break;
        default:
            Print(L"[CPU] MOV Sreg con reg=%d no implementado\n", m.reg);
            break;
    }

    return;
    }
    case 0x89: {   // MOV r/m16, r16
    uint8_t modrm_byte = fetch8();
    ModRM m = decode_modrm(modrm_byte);

    uint16_t src = get_reg16(m.reg);

    if (m.mod == 3) {
        set_reg16(m.rm, src);
        return;
    }

    uint32_t ea = ea16(m);
    uint16_t seg = cpu.segment_override ? cpu.override_seg : cpu.ds;
    cpu.segment_override = 0;   // se consume el override
    uint32_t addr = (seg << 4) + ea;

    mem_write16(addr, src);
    return;
    }


    // INC reg16
    case 0x40: inc16_reg(&cpu.eax); break;
    case 0x41: inc16_reg(&cpu.ecx); break;
    case 0x42: inc16_reg(&cpu.edx); break;
    case 0x43: inc16_reg(&cpu.ebx); break;
    case 0x44: { // INC SP
    uint16_t sp = get_sp();
    uint16_t res = sp + 1;
    set_sp(res);

    // INC no toca CF
    if (res == 0) cpu.eflags |= FLAG_ZF; else cpu.eflags &= ~FLAG_ZF;
    if (res & 0x8000) cpu.eflags |= FLAG_SF; else cpu.eflags &= ~FLAG_SF;
    if (sp == 0x7FFF) cpu.eflags |= FLAG_OF; else cpu.eflags &= ~FLAG_OF;

    return;
    }

    case 0x45: inc16_reg(&cpu.ebp); break;
    case 0x47: inc16_reg(&cpu.edi); break;
    case 0x53: { // PUSH BX
    uint16_t sp = get_sp() - 2;
    set_sp(sp);
    uint16_t bx = get_bx();
    mem_write16((cpu.ss << 4) + sp, bx);
    return;
    }
    case 0x31: {   // XOR r/m16, r16
    uint8_t modrm = fetch8();
    ModRM m = decode_modrm(modrm);

    uint16_t src = get_reg16(m.reg);
    uint16_t dst;

    if (m.mod == 3) {
        dst = get_reg16(m.rm);
        uint16_t result = dst ^ src;
        set_reg16(m.rm, result);

        cpu.eflags &= ~(FLAG_CF | FLAG_OF | FLAG_SF | FLAG_ZF);
        if (result == 0) cpu.eflags |= FLAG_ZF;
        if (result & 0x8000) cpu.eflags |= FLAG_SF;

    } else {
        uint32_t ea = ea16(m);
        uint32_t addr = segaddr(cpu.ds, ea);

        dst = mem_read16(addr);
        uint16_t result = dst ^ src;
        mem_write16(addr, result);

        cpu.eflags &= ~(FLAG_CF | FLAG_OF | FLAG_SF | FLAG_ZF);
        if (result == 0) cpu.eflags |= FLAG_ZF;
        if (result & 0x8000) cpu.eflags |= FLAG_SF;
    }

    return;
   }
   case 0x30: {   // XOR r/m8, r8
    uint8_t modrm = fetch8();
    ModRM m = decode_modrm(modrm);

    uint8_t src = get_reg8(m.reg);
    uint8_t dst;

    if (m.mod == 3) {
        dst = get_reg8(m.rm);
        uint8_t result = dst ^ src;
        set_reg8(m.rm, result);

        cpu.eflags &= ~(FLAG_CF | FLAG_OF | FLAG_SF | FLAG_ZF);
        if (result == 0) cpu.eflags |= FLAG_ZF;
        if (result & 0x80) cpu.eflags |= FLAG_SF;

    } else {
        uint32_t ea = ea16(m);
        uint32_t addr = segaddr(cpu.ds, ea);

        dst = mem_read8(addr);
        uint8_t result = dst ^ src;
        mem_write8(addr, result);

        cpu.eflags &= ~(FLAG_CF | FLAG_OF | FLAG_SF | FLAG_ZF);
        if (result == 0) cpu.eflags |= FLAG_ZF;
        if (result & 0x80) cpu.eflags |= FLAG_SF;
    }

    return;
   }
   case 0x1A: {   // SBB r8, r/m8
    uint8_t modrm = fetch8();
    ModRM m = decode_modrm(modrm);

    uint8_t src;
    if (m.mod == 3)
        src = get_reg8(m.rm);
    else
        src = mem_read8(segaddr(cpu.ds, ea16(m)));

    uint8_t dst = get_reg8(m.reg);
    uint8_t cf  = (cpu.eflags & FLAG_CF) ? 1 : 0;

    uint16_t result = (uint16_t)dst - src - cf;

    set_reg8(m.reg, (uint8_t)result);

    // FLAGS
    cpu.eflags = (cpu.eflags & ~(FLAG_CF | FLAG_ZF | FLAG_SF | FLAG_OF))
               | ((result & 0x100) ? FLAG_CF : 0)
               | (((uint8_t)result == 0) ? FLAG_ZF : 0)
               | (((uint8_t)result & 0x80) ? FLAG_SF : 0)
               | ((((dst ^ src) & (dst ^ result)) & 0x80) ? FLAG_OF : 0);

    return;
}
case 0x1B: {   // SBB r16, r/m16
    uint8_t modrm = fetch8();
    ModRM m = decode_modrm(modrm);

    uint16_t src;
    if (m.mod == 3)
        src = get_reg16(m.rm);
    else
        src = mem_read16(segaddr(cpu.ds, ea16(m)));

    uint16_t dst = get_reg16(m.reg);
    uint16_t cf  = (cpu.eflags & FLAG_CF) ? 1 : 0;

    uint32_t result = (uint32_t)dst - src - cf;

    set_reg16(m.reg, (uint16_t)result);

    // FLAGS
    cpu.eflags = (cpu.eflags & ~(FLAG_CF | FLAG_ZF | FLAG_SF | FLAG_OF))
               | ((result & 0x10000) ? FLAG_CF : 0)
               | (((uint16_t)result == 0) ? FLAG_ZF : 0)
               | (((uint16_t)result & 0x8000) ? FLAG_SF : 0)
               | ((((dst ^ src) & (dst ^ result)) & 0x8000) ? FLAG_OF : 0);

    return;
}
case 0x16: {   // PUSH SS
    uint16_t sp = get_sp() - 2;
    set_sp(sp);

    uint32_t addr = segaddr(cpu.ss, sp);
    mem_write16(addr, cpu.ss);

    return;
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

    // JNZ rel8 (75 cb)

    // JMP rel8 (EB cb)
    case 0xEB: {   // JMP rel8
    int8_t rel = fetch8();   // lee el offset y avanza IP
    set_ip(get_ip() + rel);  // salta desde IP correcto
    return;
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
                uint32_t src = segaddr(cpu.ds, get_si());

                uint32_t dst = segaddr(cpu.es, get_di());


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
                uint32_t dst = segaddr(cpu.es, get_di());

                mem_write8(dst, get_ax() & 0xFF); // AL

                set_di(get_di() + 1);
                set_cx(get_cx() - 1);
            }
            return;
        }

	case 0xA5: { // REP MOVSW
        uint16_t cx = get_cx();
        if (cx == 0) return;

        uint16_t si = get_si();
        uint16_t di = get_di();

        uint16_t ds = cpu.ds;
        uint16_t es = cpu.es;

        while (cx--) {
            uint16_t val = mem_read16((ds << 4) + si);
            mem_write16((es << 4) + di, val);

            if (cpu.eflags & FLAG_DF) {
                si -= 2;
                di -= 2;
            } else {
                si += 2;
                di += 2;
            }
        }

        set_si(si);
        set_di(di);
        set_cx(0);
        return;
        }

        default:
            Print(L"[CPU] REP con opcode %02x no implementado\n", next);
            return;
        }
    }
    case 0x8D: {   // LEA r16, r/m16
    uint8_t modrm = fetch8();
    ModRM m = decode_modrm(modrm);

    if (m.mod == 3) {
        Print(L"[CPU] LEA con mod=3 no válido\n");
        return;
    }

    uint32_t ea = ea16(m);      // calcula la EA (offset)
    set_reg16(m.reg, (uint16_t)ea);

    return;
}
case 0x80: {   // 80 /r ib  (Group 1, r/m8, imm8)
    uint8_t modrm = fetch8();
    ModRM m = decode_modrm(modrm);

    uint8_t imm = fetch8();

    uint8_t dst;
    uint32_t ea, addr;

    if (m.mod == 3) {
        dst = get_reg8(m.rm);
    } else {
        ea   = ea16(m);
        addr = segaddr(cpu.ds, ea);
        dst  = mem_read8(addr);
    }

    uint8_t result;

    switch (m.reg) {

        case 0: // ADD r/m8, imm8
            result = dst + imm;

            // CF
            set_flag(FLAG_CF, result < dst);

            // ZF
            set_flag(FLAG_ZF, result == 0);

            // SF
            set_flag(FLAG_SF, (result & 0x80) != 0);

            // OF
            set_flag(FLAG_OF,
                (((dst ^ imm ^ 0x80) & (result ^ imm)) & 0x80) != 0
            );
            break;

        case 1: // OR r/m8, imm8
            result = dst | imm;

            cpu.eflags &= ~(FLAG_CF | FLAG_OF);
            set_flag(FLAG_ZF, result == 0);
            set_flag(FLAG_SF, (result & 0x80) != 0);
            break;

        case 2: { // ADC r/m8, imm8
            uint8_t carry = (cpu.eflags & FLAG_CF) ? 1 : 0;
            uint16_t tmp = dst + imm + carry;
            result = (uint8_t)tmp;

            set_flag(FLAG_CF, tmp > 0xFF);
            set_flag(FLAG_ZF, result == 0);
            set_flag(FLAG_SF, (result & 0x80) != 0);

            set_flag(FLAG_OF,
                (((dst ^ imm ^ 0x80) & (result ^ imm)) & 0x80) != 0
            );
            break;
        }

        case 3: { // SBB r/m8, imm8
            uint8_t carry = (cpu.eflags & FLAG_CF) ? 1 : 0;
            uint16_t tmp = dst - imm - carry;
            result = (uint8_t)tmp;

            set_flag(FLAG_CF, tmp > 0xFF);
            set_flag(FLAG_ZF, result == 0);
            set_flag(FLAG_SF, (result & 0x80) != 0);

            set_flag(FLAG_OF,
                (((dst ^ imm) & (dst ^ result)) & 0x80) != 0
            );
            break;
        }

        case 4: // AND r/m8, imm8
            result = dst & imm;
            cpu.eflags &= ~(FLAG_CF | FLAG_OF);
            set_flag(FLAG_ZF, result == 0);
            set_flag(FLAG_SF, (result & 0x80) != 0);
            break;

        case 5: // SUB r/m8, imm8
            result = dst - imm;
            set_sub_flags8(dst, imm, result);
            break;

        case 6: // XOR r/m8, imm8
            result = dst ^ imm;
            cpu.eflags &= ~(FLAG_CF | FLAG_OF);
            set_flag(FLAG_ZF, result == 0);
            set_flag(FLAG_SF, (result & 0x80) != 0);
            break;

        case 7: // CMP r/m8, imm8
            result = dst - imm;
            set_sub_flags8(dst, imm, result);
            return; // NO escribir resultado
    }

    // write back
    if (m.mod == 3)
        set_reg8(m.rm, result);
    else
        mem_write8(addr, result);

    return;
}


    case 0x3C: { // CMP AL, imm8
    uint8_t imm = fetch8();
    uint8_t al  = get_ax() & 0xFF;
    uint8_t res = al - imm;

    // CF
    if (al < imm) cpu.eflags |= FLAG_CF;
    else          cpu.eflags &= ~FLAG_CF;

    // ZF
    if (res == 0) cpu.eflags |= FLAG_ZF;
    else          cpu.eflags &= ~FLAG_ZF;

    // SF
    if (res & 0x80) cpu.eflags |= FLAG_SF;
    else            cpu.eflags &= ~FLAG_SF;

    // OF (para SUB: (a^b)&(a^r)&0x80)
    uint8_t a = al;
    uint8_t b = imm;
    uint8_t r = res;
    if (((a ^ b) & (a ^ r) & 0x80) != 0) cpu.eflags |= FLAG_OF;
    else                                cpu.eflags &= ~FLAG_OF;

    return;
    }


    // MOVSB (A4)
    case 0xA4: {
        uint32_t src = segaddr(cpu.ds, get_si());

        uint32_t dst = segaddr(cpu.es, get_di());

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

    // 40–47: INC r16
    case 0x46: { // INC SI
        uint16_t si  = get_si();
        uint16_t res = si + 1;
        set_si(res);

        // INC no toca CF
        if (res == 0) cpu.eflags |= FLAG_ZF; else cpu.eflags &= ~FLAG_ZF;
        if (res & 0x8000) cpu.eflags |= FLAG_SF; else cpu.eflags &= ~FLAG_SF;
        // OF: de 7FFF -> 8000
        if (si == 0x7FFF) cpu.eflags |= FLAG_OF; else cpu.eflags &= ~FLAG_OF;

        return;
    }

    // 48–4F: DEC r16
    case 0x4F: { // DEC DI
        uint16_t di  = get_di();
        uint16_t res = di - 1;
        set_di(res);

        // DEC no toca CF
        if (res == 0) cpu.eflags |= FLAG_ZF; else cpu.eflags &= ~FLAG_ZF;
        if (res & 0x8000) cpu.eflags |= FLAG_SF; else cpu.eflags &= ~FLAG_SF;
        // OF: de 8000 -> 7FFF
        if (di == 0x8000) cpu.eflags |= FLAG_OF; else cpu.eflags &= ~FLAG_OF;

        return;
    }


// ======================================================
//  SALTOS CONDICIONALES 70h–7Fh (rel8)
// ======================================================

case 0x70: { int8_t rel = fetch8(); if (cpu.eflags & FLAG_OF) set_ip(get_ip()+rel); return; }
case 0x71: { int8_t rel = fetch8(); if (!(cpu.eflags & FLAG_OF)) set_ip(get_ip()+rel); return; }
case 0x72: { int8_t rel = fetch8(); if (cpu.eflags & FLAG_CF) set_ip(get_ip()+rel); return; }
case 0x73: { int8_t rel = fetch8(); if (!(cpu.eflags & FLAG_CF)) set_ip(get_ip()+rel); return; }
case 0x74: { int8_t rel = fetch8(); if (cpu.eflags & FLAG_ZF) set_ip(get_ip()+rel); return; }
case 0x75: { int8_t rel = fetch8(); if (!(cpu.eflags & FLAG_ZF)) set_ip(get_ip()+rel); return; }
case 0x76: { int8_t rel = fetch8(); if ((cpu.eflags & FLAG_CF)||(cpu.eflags & FLAG_ZF)) set_ip(get_ip()+rel); return; }
case 0x77: { int8_t rel = fetch8(); if (!(cpu.eflags & FLAG_CF)&&!(cpu.eflags & FLAG_ZF)) set_ip(get_ip()+rel); return; }
case 0x78: { int8_t rel = fetch8(); if (cpu.eflags & FLAG_SF) set_ip(get_ip()+rel); return; }
case 0x79: { int8_t rel = fetch8(); if (!(cpu.eflags & FLAG_SF)) set_ip(get_ip()+rel); return; }
case 0x7A: { int8_t rel = fetch8(); if (cpu.eflags & FLAG_PF) set_ip(get_ip()+rel); return; }
case 0x7B: { int8_t rel = fetch8(); if (!(cpu.eflags & FLAG_PF)) set_ip(get_ip()+rel); return; }
case 0x7C: { int8_t rel = fetch8(); bool sf=(cpu.eflags&FLAG_SF)!=0, of=(cpu.eflags&FLAG_OF)!=0; if (sf!=of) set_ip(get_ip()+rel); return; }
case 0x7D: { int8_t rel = fetch8(); bool sf=(cpu.eflags&FLAG_SF)!=0, of=(cpu.eflags&FLAG_OF)!=0; if (sf==of) set_ip(get_ip()+rel); return; }
case 0x7E: { int8_t rel = fetch8(); bool sf=(cpu.eflags&FLAG_SF)!=0, of=(cpu.eflags&FLAG_OF)!=0; if ((cpu.eflags&FLAG_ZF)||(sf!=of)) set_ip(get_ip()+rel); return; }
case 0x7F: { int8_t rel = fetch8(); bool sf=(cpu.eflags&FLAG_SF)!=0, of=(cpu.eflags&FLAG_OF)!=0; if (!(cpu.eflags&FLAG_ZF)&&(sf==of)) set_ip(get_ip()+rel); return; }


    case 0x65: {   // GS: segment override prefix
    // De momento lo ignoramos (FreeDOS no usa GS realmente)
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

       uint16_t ax = cpu.eax & 0xFFFF;
       uint16_t bx = cpu.ebx & 0xFFFF;
       uint16_t cx = cpu.ecx & 0xFFFF;
       uint16_t dx = cpu.edx & 0xFFFF;
       uint16_t si = cpu.esi & 0xFFFF;
       uint16_t di = cpu.edi & 0xFFFF;

       Print(L"Regs: AX=%04x BX=%04x CX=%04x DX=%04x SI=%04x DI=%04x DS=%04x ES=%04x\n",
      ax, bx, cx, dx, si, di, cpu.ds, cpu.es);

    }
}
