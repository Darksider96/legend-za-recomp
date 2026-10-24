// Testes do leitor de NSO: SHA-256 (vetores FIPS 180-2), LZ4 de bloco e, com o dump, o exefs/main real
// uso: nso_test [pasta_exefs]

#include "recomp/nso.hpp"

#include <cstdio>
#include <filesystem>
#include <string>
#include <vector>

namespace {

int g_failures = 0;

void Check(bool condition, const char* what) {
    std::printf("%s %s\n", condition ? "[PASS]" : "[FAIL]", what);
    if (!condition) ++g_failures;
}

std::string HexDigest(std::span<const uint8_t> data) {
    const auto digest = recomp::Sha256(data);
    std::string out;
    char buffer[3];
    for (uint8_t b : digest) {
        std::snprintf(buffer, sizeof(buffer), "%02x", b);
        out += buffer;
    }
    return out;
}

std::span<const uint8_t> Bytes(const std::string& s) { return {reinterpret_cast<const uint8_t*>(s.data()), s.size()}; }

void TestSha256() {
    const std::string million(1000000, 'a');
    Check(HexDigest(Bytes("")) == "e3b0c44298fc1c149afbf4c8996fb92427ae41e4649b934ca495991b7852b855" &&
              HexDigest(Bytes("abc")) == "ba7816bf8f01cfea414140de5dae2223b00361a396177a9cb410ff61f20015ad" &&
              HexDigest(Bytes("abcdbcdecdefdefgefghfghighijhijkijkljklmklmnlmnomnopnopq")) ==
                  "248d6a61d20638b8e5c026930c3e6039a33ce45964ff2167f6ecedd419db06c1" &&
              HexDigest(Bytes(million)) == "cdc76e5c9914fb9281a1c7e284d73e67f1809a48a497200e046d39ccc7112cd0",
          "SHA-256: vetores FIPS 180-2 (vazio, abc, 448 bits, 1 milhao de 'a')");
}

void TestLz4() {
    // 3 literais + match de 9 bytes a distancia 3 (copia sobreposta)
    const std::vector<uint8_t> overlap = {0x35, 'a', 'b', 'c', 0x03, 0x00};
    std::vector<uint8_t> out(12);
    Check(recomp::Lz4DecompressBlock(overlap, out) && std::string(out.begin(), out.end()) == "abcabcabcabc",
          "LZ4: match sobreposto (offset 3, 9 bytes)");
    // 20 literais (comprimento estendido 15 + 5)
    std::vector<uint8_t> literal = {0xF0, 5};
    for (int i = 0; i < 20; ++i) literal.push_back(static_cast<uint8_t>('A' + i));
    std::vector<uint8_t> out20(20);
    Check(recomp::Lz4DecompressBlock(literal, out20) && out20.front() == 'A' && out20.back() == 'T', "LZ4: comprimento de literais estendido");
    std::vector<uint8_t> small(11);
    const std::vector<uint8_t> bad_offset = {0x15, 'a', 0x05, 0x00};
    std::vector<uint8_t> out6(6);
    Check(!recomp::Lz4DecompressBlock(overlap, small) && !recomp::Lz4DecompressBlock(bad_offset, out6),
          "LZ4: rejeita saida com tamanho diferente e offset antes do inicio");
}

void TestRealModule(const std::filesystem::path& exefs) {
    recomp::NsoModule main_module, sdk;
    std::string error;
    const bool loaded = main_module.Load((exefs / "main").string(), error);
    Check(loaded && main_module.Text().size == 50098768 && main_module.Rodata().size == 10588160 && main_module.Data().size == 3793344,
          "exefs/main: segmentos descomprimidos com SHA-256 conferido (50.098.768 / 10.588.160 / 3.793.344 bytes)");
    if (!loaded) {
        std::printf("       %s\n", error.c_str());
        return;
    }
    const recomp::Symbol* nn_main = main_module.FindExport("nnMain");
    Check(nn_main && nn_main->value == 0x19E8950 && main_module.FindExport("nninitStartup") &&
              main_module.FindExport("nninitStartup")->value == 0x19E8720,
          "nnMain (0x19E8950) e nninitStartup (0x19E8720) exportados como no relatorio de boot");
    bool sorted = true;
    const auto& functions = main_module.Functions();
    for (size_t i = 1; i < functions.size(); ++i) sorted = sorted && functions[i - 1].start <= functions[i].start;
    bool in_text = !functions.empty();
    for (const auto& f : functions) in_text = in_text && f.start + f.size <= main_module.Text().size;
    Check(sorted && in_text && functions.size() > 10000, ".eh_frame: funcoes ordenadas e todas dentro do .text");
    Check(sdk.Load((exefs / "sdk").string(), error) && !sdk.Symbols().empty(), "exefs/sdk carregado com SHA-256 conferido");
}

} // namespace

int main(int argc, char** argv) {
    TestSha256();
    TestLz4();
    if (argc > 1) {
        TestRealModule(argv[1]);
    } else {
        std::puts("(sem pasta exefs: testes com o dump ignorados)");
    }
    std::printf("%s (%d falhas)\n", g_failures == 0 ? "OK" : "FALHOU", g_failures);
    return g_failures == 0 ? 0 : 1;
}
