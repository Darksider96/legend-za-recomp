#pragma once
// Decodificador AArch64 (A64) do recompilador — classes inteiras do ARMv8.0-A
//
// Cobre: imediatos (add/sub, logico, move wide, bitfield, extract, adr/adrp), ramificacoes (b, bl, b.cond, cbz/cbnz,
// tbz/tbnz, br/blr/ret), loads/stores inteiros (offset sem sinal, pre/pos-indice, sem escala, registrador, par,
// literal), operacoes com registradores (logico, add/sub deslocado/estendido, adc/sbc, csel, ccmp, 1/2/3 fontes) e
// sistema (hint, barreiras, mrs/msr, svc, brk, udf), exclusivos (ldxr/stxr, pares) e acquire/release (ldar/stlr).
// SIMD/FP fica como Op::Unknown (fase R3); CAS/LSE da v8.1 nao existem no Cortex-A57 do Switch.

#include <cstdint>
#include <string>

namespace recomp::a64 {

enum class Op : uint8_t {
    Unknown,
    // Imediatos
    AddImm, SubImm, MovZ, MovN, MovK, AndImm, OrrImm, EorImm, Adr, Adrp, Sbfm, Bfm, Ubfm, Extr,
    // Ramificacoes
    B, Bl, BCond, Cbz, Cbnz, Tbz, Tbnz, Br, Blr, Ret,
    // Loads/stores
    Load, Store, LoadPair, StorePair, Prefetch,
    LoadExclusive, StoreExclusive, LoadExclusivePair, StoreExclusivePair, LoadAcquire, StoreRelease,
    // Registradores
    And, Bic, Orr, Orn, Eor, Eon,          // set_flags em And/Bic = ANDS/BICS
    AddShift, SubShift, AddExt, SubExt, Adc, Sbc,
    Csel, Csinc, Csinv, Csneg, Ccmp, Ccmn,
    Udiv, Sdiv, Lslv, Lsrv, Asrv, Rorv,
    Rbit, Rev16, Rev32, Rev, Clz, Cls,
    Madd, Msub, Smaddl, Smsubl, Umaddl, Umsubl, Smulh, Umulh,
    // Sistema
    Hint, Barrier, Clrex, Mrs, Msr, Svc, Brk, Udf,
};

enum class AddrMode : uint8_t { Offset, PreIndex, PostIndex, Register, Literal };

struct Insn {
    Op op{Op::Unknown};
    uint32_t word{0};
    uint64_t pc{0};          // RVA
    uint8_t rd{0}, rn{0}, rm{0}, ra{0};   // ra tambem e Rt2 em pares; rm e Rs (status) em store exclusivo
    bool sf{false};          // operacao de 64 bits
    bool set_flags{false};
    int64_t imm{0};          // imediato ja decodificado (deslocado/estendido)
    uint8_t shift{0};        // tipo: 0 LSL, 1 LSR, 2 ASR, 3 ROR; ou hw*16 em move wide
    uint8_t amount{0};       // quantidade do deslocamento / imm3 de extend
    uint8_t extend{0};       // option do extend (UXTB..SXTX)
    uint8_t cond{0};
    uint8_t immr{0}, imms{0};
    uint8_t bit{0};          // tbz/tbnz
    // Memoria
    uint8_t size{0};         // log2 dos bytes acessados (0..3)
    bool sign_extend{false}; // LDRSB/LDRSH/LDRSW
    bool dest64{true};       // destino X (true) ou W (false) em loads com sinal
    bool ordered{false};     // exclusivos: acquire (LDAXR) / release (STLXR); sempre true em LDAR/STLR
    AddrMode mode{AddrMode::Offset};
    uint64_t target{0};      // RVA de destino (ramificacoes, adr/adrp e literais)
    uint16_t sysreg{0};      // mrs/msr: o0:op1:CRn:CRm:op2
};

Insn Decode(uint32_t word, uint64_t pc);

const char* OpName(Op op);
// Texto curto para logs e diagnosticos (nao e a sintaxe completa do assembler)
std::string Describe(const Insn& insn);

// System registers usados por jogos do Switch
inline constexpr uint16_t kSysTpidrEl0 = 0x5E82;
inline constexpr uint16_t kSysTpidrroEl0 = 0x5E83;
inline constexpr uint16_t kSysCntvctEl0 = 0x5F02;
inline constexpr uint16_t kSysCntfrqEl0 = 0x5F00;
inline constexpr uint16_t kSysFpcr = 0x5A20;
inline constexpr uint16_t kSysFpsr = 0x5A21;
inline constexpr uint16_t kSysDczidEl0 = 0x5807;

} // namespace recomp::a64
