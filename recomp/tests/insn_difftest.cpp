// insn_difftest — executa os testes por instrucao gerados por a64_insn_tests sobre estados iniciais dados
//
// uso: insn_difftest <main> <casos.bin> <resultados.bin>
//   Mapeia a imagem do main em kImageBase e a area de rascunho em kScratchBase (os mesmos enderecos que o emulador
//   de referencia usa), roda cada caso e grava o estado final. A comparacao fica em recomp/tools/insn_difftest.py.
//   Formato dos registros: ver CaseRecord/ResultRecord (little-endian, sem padding).

#include "insn_difftest.hpp"
#include "recomp/nso.hpp"

#include <windows.h>

#include <algorithm>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <fstream>
#include <iostream>
#include <vector>

namespace {

constexpr uint64_t kScratchBase = 0x7000000000ull;
constexpr uint64_t kScratchSize = 0x11000;

#pragma pack(push, 1)
struct CaseRecord {
    uint64_t rva;
    uint64_t x[31];
    uint64_t sp;
    uint32_t nzcv;
    uint32_t fpcr;
    uint64_t tpidr, tpidrro;
    uint8_t v[32][16];
};
struct ResultRecord {
    uint64_t x[31];
    uint64_t sp;
    uint32_t nzcv;
    uint32_t fpcr;
    uint8_t v[32][16];
    uint64_t exit;              // primeiro destino de saida; 1 = retornou (RET)
    uint8_t scratch_sha256[32];
};
#pragma pack(pop)

uint64_t g_exit = 0;

void* MapAt(uint64_t address, uint64_t size) {
    return VirtualAlloc(reinterpret_cast<void*>(address), size, MEM_RESERVE | MEM_COMMIT, PAGE_READWRITE);
}

// Conteudo inicial da area de rascunho: splitmix64 do indice de cada palavra (o harness gera o mesmo)
std::vector<uint8_t> ScratchPattern() {
    std::vector<uint8_t> bytes(kScratchSize);
    for (uint64_t i = 0; i < kScratchSize / 8; ++i) {
        uint64_t z = (i + 1) * 0x9E3779B97F4A7C15ull;
        z = (z ^ (z >> 30)) * 0xBF58476D1CE4E5B9ull;
        z = (z ^ (z >> 27)) * 0x94D049BB133111EBull;
        z ^= z >> 31;
        std::memcpy(bytes.data() + i * 8, &z, 8);
    }
    return bytes;
}

}  // namespace

namespace recomp {

void test::Exit(uint64_t guest_address) {
    if (g_exit == 0) g_exit = guest_address;
}

void Dispatch(Context&, uint64_t guest_address) { test::Exit(guest_address); }

void Unimplemented(Context&, uint64_t rva, uint32_t word) {
    std::fprintf(stderr, "instrucao nao traduzida em 0x%llx (0x%08x)\n", static_cast<unsigned long long>(rva), word);
    std::abort();
}

void Trap(Context&, uint64_t rva, uint32_t imm) {
    std::fprintf(stderr, "BRK/UDF #%u em 0x%llx\n", imm, static_cast<unsigned long long>(rva));
    std::abort();
}

uint64_t ReadCounter() { return 0; }

}  // namespace recomp

int main(int argc, char** argv) {
    if (argc != 4) {
        std::cerr << "uso: insn_difftest <main> <casos.bin> <resultados.bin>\n";
        return 2;
    }
    recomp::NsoModule module;
    std::string error;
    if (!module.Load(argv[1], error)) {
        std::cerr << error << "\n";
        return 2;
    }
    const auto image = module.Image();
    auto* guest_image = static_cast<uint8_t*>(MapAt(recomp::kImageBase, image.size()));
    auto* scratch = static_cast<uint8_t*>(MapAt(kScratchBase, kScratchSize));
    if (guest_image != reinterpret_cast<uint8_t*>(recomp::kImageBase) || scratch != reinterpret_cast<uint8_t*>(kScratchBase)) {
        std::cerr << "nao foi possivel mapear a memoria do jogo nos enderecos fixos\n";
        return 2;
    }
    std::memcpy(guest_image, image.data(), image.size());
    const std::vector<uint8_t> pristine = ScratchPattern();

    std::ifstream in(argv[2], std::ios::binary);
    std::ofstream out(argv[3], std::ios::binary);
    const recomp::test::InsnTest* begin = recomp::test::kInsnTests;
    const recomp::test::InsnTest* end = begin + recomp::test::kInsnTestCount;
    CaseRecord in_case;
    size_t cases = 0;
    while (in.read(reinterpret_cast<char*>(&in_case), sizeof(in_case))) {
        const auto* test = std::lower_bound(begin, end, in_case.rva, [](const recomp::test::InsnTest& t, uint64_t rva) { return t.rva < rva; });
        if (test == end || test->rva != in_case.rva) {
            std::cerr << "teste ausente para 0x" << std::hex << in_case.rva << "\n";
            return 1;
        }
        recomp::Context c;
        std::memcpy(c.x, in_case.x, sizeof(in_case.x));
        c.sp = in_case.sp;
        c.n = (in_case.nzcv >> 31) & 1;
        c.z = (in_case.nzcv >> 30) & 1;
        c.c = (in_case.nzcv >> 29) & 1;
        c.v = (in_case.nzcv >> 28) & 1;
        c.fpcr = in_case.fpcr;
        c.tpidr = in_case.tpidr;
        c.tpidrro = in_case.tpidrro;
        std::memcpy(c.vreg, in_case.v, sizeof(in_case.v));
        std::memcpy(scratch, pristine.data(), kScratchSize);
        g_exit = 0;

        test->fn(c);

        ResultRecord r{};
        std::memcpy(r.x, c.x, sizeof(r.x));
        r.sp = c.sp;
        r.nzcv = (uint32_t{c.n} << 31) | (uint32_t{c.z} << 30) | (uint32_t{c.c} << 29) | (uint32_t{c.v} << 28);
        r.fpcr = c.fpcr;
        std::memcpy(r.v, c.vreg, sizeof(r.v));
        r.exit = g_exit == 0 ? 1 : g_exit;
        const auto digest = recomp::Sha256({scratch, kScratchSize});
        std::memcpy(r.scratch_sha256, digest.data(), digest.size());
        out.write(reinterpret_cast<const char*>(&r), sizeof(r));
        ++cases;
    }
    std::cout << cases << " casos executados\n";
    return out ? 0 : 1;
}
