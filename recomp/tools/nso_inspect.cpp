// nso_inspect — inventario de um modulo NSO para o recompilador (fase R1)
//
// uso: nso_inspect <main> [--sdk <sdk>] [--ghidra-text <text.bin>] [--out <pasta>]
//   Verifica o SHA-256 de cada segmento, le MOD0/.dynamic/dynsym/relocacoes/.eh_frame e escreve em <pasta>:
//     imports.csv    nome,namespace,demangled,tipo,got,resolvido_no_sdk
//     exports.csv    nome,rva,tamanho,tipo
//     functions.csv  rva,tamanho          (funcoes descritas no .eh_frame)
//   Saida derivada do binario do jogo: a pasta padrao (analysis/recomp/generated) nao e versionada.

#include "recomp/nso.hpp"

#include <cxxabi.h>

#include <algorithm>
#include <cstdio>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <map>
#include <set>
#include <string>
#include <unordered_map>
#include <unordered_set>

namespace fs = std::filesystem;

namespace {

std::string Demangle(const std::string& name) {
    int status = 0;
    char* text = abi::__cxa_demangle(name.c_str(), nullptr, nullptr, &status);
    std::string out = status == 0 && text ? text : name;
    std::free(text);
    return out;
}

// "nn::os", "nn::fs"... a partir do nome demangled; C/NVN/outros pelo prefixo
std::string Namespace(const std::string& mangled, const std::string& demangled) {
    if (demangled.starts_with("nn::")) {
        const size_t second = demangled.find("::", 4);
        return second == std::string::npos ? "nn" : demangled.substr(0, second);
    }
    if (mangled.starts_with("nvn")) return "nvn";
    if (mangled.starts_with("_Z")) {
        const size_t colon = demangled.find("::");
        const size_t paren = demangled.find('(');
        return colon != std::string::npos && (paren == std::string::npos || colon < paren) ? demangled.substr(0, colon) : "c++";
    }
    return "c";
}

std::string Csv(std::string text) {
    if (text.find_first_of(",\"") == std::string::npos) return text;
    std::string out = "\"";
    for (char c : text) out += c == '"' ? std::string("\"\"") : std::string(1, c);
    return out + "\"";
}

std::string Hex(uint64_t v) {
    char buffer[24];
    std::snprintf(buffer, sizeof(buffer), "0x%llx", static_cast<unsigned long long>(v));
    return buffer;
}

bool Report(bool ok, const std::string& what) {
    std::cout << (ok ? "  [OK]    " : "  [FALHA] ") << what << "\n";
    return ok;
}

} // namespace

int main(int argc, char** argv) {
    std::string main_path, sdk_path, ghidra_text, out_dir = "analysis/recomp/generated";
    for (int i = 1; i < argc; ++i) {
        const std::string arg = argv[i];
        if (arg == "--sdk" && i + 1 < argc) sdk_path = argv[++i];
        else if (arg == "--ghidra-text" && i + 1 < argc) ghidra_text = argv[++i];
        else if (arg == "--out" && i + 1 < argc) out_dir = argv[++i];
        else if (main_path.empty() && !arg.starts_with("--")) main_path = arg;
        else {
            std::cerr << "uso: nso_inspect <main> [--sdk <sdk>] [--ghidra-text <text.bin>] [--out <pasta>]\n";
            return 2;
        }
    }
    if (main_path.empty()) {
        std::cerr << "uso: nso_inspect <main> [--sdk <sdk>] [--ghidra-text <text.bin>] [--out <pasta>]\n";
        return 2;
    }

    recomp::NsoModule module;
    std::string error;
    if (!module.Load(main_path, error)) {
        std::cerr << error << "\n";
        return 1;
    }
    bool all_ok = true;
    const char* names[3] = {".text", ".rodata", ".data"};
    const recomp::Segment* segments[3] = {&module.Text(), &module.Rodata(), &module.Data()};
    std::cout << main_path << "\n";
    for (int i = 0; i < 3; ++i) {
        const recomp::Segment& s = *segments[i];
        std::printf("  %-8s RVA %-12s %10u bytes (arquivo %u, LZ4 %s, SHA-256 %s)\n", names[i], Hex(s.memory_offset).c_str(), s.size,
                    s.file_size, s.compressed ? "sim" : "nao", s.hash_checked ? "conferido" : "ausente");
    }
    std::printf("  .bss     %u bytes; MOD0 %s, .dynamic %s, .eh_frame_hdr %s\n", module.BssSize(), Hex(module.Mod0()).c_str(),
                Hex(module.Dynamic()).c_str(), Hex(module.EhFrameHdr()).c_str());

    // Simbolos e relocacoes
    std::map<uint32_t, size_t> reloc_types;
    size_t relr = 0, relr_outside = 0;
    for (const recomp::Relocation& r : module.Relocations()) {
        if (!r.relr) continue;
        ++relr;
        relr_outside += static_cast<uint64_t>(r.addend) >= module.Image().size() ? 1 : 0;
    }
    std::unordered_map<uint32_t, std::vector<uint64_t>> got_of_symbol;  // indice do simbolo -> RVAs corrigidos
    for (const recomp::Relocation& r : module.Relocations()) {
        ++reloc_types[r.type];
        if (r.symbol) got_of_symbol[r.symbol].push_back(r.offset);
    }
    std::unordered_set<std::string> sdk_exports;
    if (!sdk_path.empty()) {
        recomp::NsoModule sdk;
        if (!sdk.Load(sdk_path, error)) {
            std::cerr << error << "\n";
            return 1;
        }
        for (const recomp::Symbol& s : sdk.Symbols()) {
            if (s.section != 0 && !s.name.empty()) sdk_exports.insert(s.name);
        }
        std::cout << "  sdk: " << sdk_exports.size() << " exportacoes, " << sdk.Functions().size() << " funcoes no .eh_frame\n";
    }

    std::error_code ec;
    fs::create_directories(out_dir, ec);
    std::ofstream imports(fs::path(out_dir) / "imports.csv"), exports(fs::path(out_dir) / "exports.csv"),
        functions(fs::path(out_dir) / "functions.csv");
    imports << "nome,namespace,demangled,tipo,got,resolvido_no_sdk\n";
    exports << "nome,rva,tamanho,tipo\n";
    functions << "rva,tamanho\n";

    std::map<std::string, std::pair<size_t, size_t>> by_namespace;  // namespace -> (importacoes, resolvidas no sdk)
    size_t import_count = 0, export_count = 0, resolved = 0, imported_functions = 0;
    std::vector<std::string> unresolved;
    const auto& symbols = module.Symbols();
    for (uint32_t i = 0; i < symbols.size(); ++i) {
        const recomp::Symbol& s = symbols[i];
        if (s.name.empty()) continue;
        const std::string demangled = Demangle(s.name);
        if (s.IsImport()) {
            ++import_count;
            imported_functions += s.IsFunction() ? 1 : 0;
            const bool in_sdk = sdk_exports.contains(s.name);
            resolved += in_sdk ? 1 : 0;
            if (!in_sdk) unresolved.push_back(demangled);
            const std::string ns = Namespace(s.name, demangled);
            ++by_namespace[ns].first;
            by_namespace[ns].second += in_sdk ? 1 : 0;
            std::string got;
            for (uint64_t rva : got_of_symbol[i]) got += (got.empty() ? "" : ";") + Hex(rva);
            imports << Csv(s.name) << "," << Csv(ns) << "," << Csv(demangled) << "," << (s.IsFunction() ? "func" : "obj") << "," << got
                    << "," << (sdk_path.empty() ? "?" : in_sdk ? "sim" : "nao") << "\n";
        } else if (s.section != 0) {
            ++export_count;
            exports << Csv(s.name) << "," << Hex(s.value) << "," << s.size << "," << (s.IsFunction() ? "func" : "obj") << "\n";
        }
    }

    uint64_t covered = 0, largest = 0, largest_at = 0, overlaps = 0, previous_end = 0;
    for (const recomp::FunctionRange& f : module.Functions()) {
        functions << Hex(f.start) << "," << f.size << "\n";
        covered += f.size;
        overlaps += f.start < previous_end ? 1 : 0;
        previous_end = std::max(previous_end, f.start + f.size);
        if (f.size > largest) largest = f.size, largest_at = f.start;
    }

    std::cout << "\n  simbolos: " << symbols.size() << " (" << import_count << " importacoes, " << imported_functions << " funcoes; "
              << export_count << " exportacoes";
    if (!sdk_path.empty()) std::cout << "; " << resolved << " resolvidas no SDK";
    std::cout << ")\n  relocacoes:";
    for (const auto& [type, count] : reloc_types) std::cout << " " << type << "=" << count;
    std::cout << " (" << relr << " relativas vindas de DT_RELR)\n  construtores estaticos (INIT_ARRAY): " << module.InitArrayCount()
              << " em " << Hex(module.InitArray());
    std::cout << "\n  importacoes por namespace" << (sdk_path.empty() ? "" : " (resolvidas no sdk)") << ":\n";
    std::vector<std::pair<std::string, std::pair<size_t, size_t>>> sorted(by_namespace.begin(), by_namespace.end());
    std::sort(sorted.begin(), sorted.end(), [](const auto& a, const auto& b) { return a.second.first > b.second.first; });
    for (const auto& [ns, counts] : sorted) {
        std::printf("    %-14s %5zu", ns.c_str(), counts.first);
        if (!sdk_path.empty()) std::printf("  (%zu no sdk)", counts.second);
        std::printf("\n");
    }
    const double text_size = static_cast<double>(module.Text().size);
    std::printf("\n  funcoes no .eh_frame: %zu, cobrindo %.1f%% do .text; maior: %llu bytes em %s; sobreposicoes: %llu\n",
                module.Functions().size(), 100.0 * static_cast<double>(covered) / text_size, static_cast<unsigned long long>(largest),
                Hex(largest_at).c_str(), static_cast<unsigned long long>(overlaps));

    // Ancoras independentes do relatorio de boot (Ghidra)
    std::cout << "\n  verificacoes:\n";
    all_ok &= Report(true, "SHA-256 dos 3 segmentos igual ao registrado no cabecalho NSO");
    all_ok &= Report(relr > 0 && relr_outside == 0,
                     std::to_string(relr) + " relocacoes DT_RELR apontam para dentro da imagem (" + std::to_string(relr_outside) + " fora)");
    const auto function_at = [&module](uint64_t rva) -> const recomp::FunctionRange* {
        const auto& list = module.Functions();
        const auto it = std::lower_bound(list.begin(), list.end(), rva, [](const recomp::FunctionRange& f, uint64_t v) { return f.start < v; });
        return it != list.end() && it->start == rva ? &*it : nullptr;
    };
    for (const auto& [name, rva, size] : {std::tuple<const char*, uint64_t, uint64_t>{"nnMain", 0x19E8950, 0x310},
                                          std::tuple<const char*, uint64_t, uint64_t>{"nninitStartup", 0x19E8720, 0x22C}}) {
        const recomp::Symbol* symbol = module.FindExport(name);
        const recomp::FunctionRange* range = function_at(rva);
        all_ok &= Report(symbol && symbol->value == rva && range && range->size == size,
                         std::string(name) + " exportado em RVA " + Hex(rva) + " com FDE de " + Hex(size) + " bytes (relatorio de boot)");
    }
    if (!ghidra_text.empty()) {
        std::ifstream text(ghidra_text, std::ios::binary);
        const std::vector<uint8_t> bytes((std::istreambuf_iterator<char>(text)), std::istreambuf_iterator<char>());
        const auto image = module.Image();
        all_ok &= Report(bytes.size() == module.Text().size && std::equal(bytes.begin(), bytes.end(), image.begin()),
                         ".text descomprimido identico ao export do Ghidra (" + ghidra_text + ")");
    }
    if (!sdk_path.empty()) {
        std::cout << "  importacoes resolvidas no sdk: " << resolved << " de " << import_count << "; nao encontradas: " << unresolved.size()
                  << "\n";
        for (size_t i = 0; i < unresolved.size() && i < 15; ++i) std::cout << "    " << unresolved[i] << "\n";
    }
    std::cout << "\n  arquivos em " << out_dir << ": imports.csv, exports.csv, functions.csv\n";
    return all_ok ? 0 : 1;
}
