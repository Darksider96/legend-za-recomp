// Prova de conceito R2: a funcao 0x133f0c0 do jogo (FNV-1a 64 de string terminada em zero, base da Game Freak),
// traduzida de AArch64 para C++ no build, comparada com za::vfs::Fnv1a64 (oraculo validado contra o data.trpfd)

#include "recomp/context.hpp"
#include "vfs/trpfs.hpp"

#include <cstdio>
#include <random>
#include <string>

namespace guest {
void f_133f0c0(recomp::Context& c);
}

namespace {

int g_failures = 0;

void Check(bool condition, const char* what) {
    std::printf("%s %s\n", condition ? "[PASS]" : "[FAIL]", what);
    if (!condition) ++g_failures;
}

uint64_t Guest(const std::string& text) {
    recomp::Context c;
    c.x[0] = reinterpret_cast<uint64_t>(text.c_str());  // memoria do jogo = memoria do host
    guest::f_133f0c0(c);
    return c.x[0];
}

} // namespace

int main() {
    Check(Guest("") == za::vfs::kFnvOffsetBasis, "string vazia devolve a base 0xCBF29CE484222645");
    const char* paths[] = {"arc/data.trpfd", "avalon/data/personal_array.bin", "ik_pokemon/data/pm0150/pm0150_00_00/pm0150_00_00.trmdl",
                           "world/ik_scene/field/area/t1_/t1_0.trscn"};
    bool known = true;
    for (const char* path : paths) known = known && Guest(path) == za::vfs::Fnv1a64(path);
    Check(known, "caminhos reais do TRPFS: hash identico ao Fnv1a64");

    std::mt19937_64 rng(20261024);
    size_t equal = 0;
    constexpr size_t kCases = 100000;
    for (size_t i = 0; i < kCases; ++i) {
        std::string text(rng() % 96, '\0');
        for (char& ch : text) ch = static_cast<char>(1 + rng() % 255);  // sem o terminador no meio
        equal += Guest(text) == za::vfs::Fnv1a64(text) ? 1 : 0;
    }
    std::printf("       %zu de %zu strings aleatorias iguais\n", equal, kCases);
    Check(equal == kCases, "100.000 strings aleatorias (bytes 1..255, ate 95 bytes): hash identico");
    std::printf("%s (%d falhas)\n", g_failures == 0 ? "OK" : "FALHOU", g_failures);
    return g_failures == 0 ? 0 : 1;
}
