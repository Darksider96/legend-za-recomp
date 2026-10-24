// a64_recomp — traduz funcoes AArch64 do main para C++ (fase R2)
//
// uso: a64_recomp <main> --function <rva> [--function <rva>...] --out <arquivo.cpp>
//      a64_recomp <main> --stats
//   --stats traduz todas as funcoes do .eh_frame em memoria e informa quantas saem sem instrucoes nao suportadas.
//   O arquivo gerado contem codigo do jogo traduzido: gravar apenas fora do repositorio (build/ ou recomp/out/).

#include "recomp/emit_c.hpp"
#include "recomp/nso.hpp"

#include <algorithm>
#include <fstream>
#include <iostream>
#include <map>
#include <string>

int main(int argc, char** argv) {
    std::string path, out;
    std::vector<uint64_t> wanted;
    bool stats = false;
    for (int i = 1; i < argc; ++i) {
        const std::string arg = argv[i];
        if (arg == "--function" && i + 1 < argc) wanted.push_back(std::stoull(argv[++i], nullptr, 0));
        else if (arg == "--out" && i + 1 < argc) out = argv[++i];
        else if (arg == "--stats") stats = true;
        else path = arg;
    }
    recomp::NsoModule module;
    std::string error;
    if (path.empty() || (!stats && (wanted.empty() || out.empty())) || !module.Load(path, error)) {
        std::cerr << (error.empty() ? "uso: a64_recomp <main> --function <rva>... --out <arquivo.cpp> | --stats" : error) << "\n";
        return 2;
    }
    const auto& functions = module.Functions();

    if (stats) {
        size_t clean = 0, instructions = 0, unsupported = 0, clean_instructions = 0;
        std::map<std::string, size_t> blockers;  // operacao nao suportada -> funcoes bloqueadas por ela
        for (const recomp::FunctionRange& f : functions) {
            recomp::EmitResult result;
            if (!recomp::EmitFunction(module, f, result)) continue;
            instructions += result.instructions;
            unsupported += result.unsupported.size();
            if (result.unsupported.empty()) {
                ++clean;
                clean_instructions += result.instructions;
            } else {
                ++blockers[recomp::a64::OpName(result.unsupported.front().op)];
            }
        }
        std::cout << functions.size() << " funcoes, " << instructions << " instrucoes\n"
                  << "  traduzidas sem pendencias: " << clean << " funcoes (" << 100.0 * static_cast<double>(clean) / static_cast<double>(functions.size())
                  << "%), " << clean_instructions << " instrucoes\n"
                  << "  instrucoes nao suportadas: " << unsupported << "\n  primeira pendencia por funcao:";
        for (const auto& [op, count] : blockers) std::cout << " " << op << "=" << count;
        std::cout << "\n";
        return 0;
    }

    std::vector<recomp::EmitResult> results;
    for (uint64_t rva : wanted) {
        const auto it = std::lower_bound(functions.begin(), functions.end(), rva,
                                         [](const recomp::FunctionRange& f, uint64_t v) { return f.start < v; });
        if (it == functions.end() || it->start != rva) {
            std::cerr << "RVA 0x" << std::hex << rva << " nao e inicio de funcao no .eh_frame\n";
            return 1;
        }
        recomp::EmitResult result;
        recomp::EmitFunction(module, *it, result);
        for (const auto& insn : result.unsupported) std::cerr << "nao suportada: " << recomp::a64::Describe(insn) << "\n";
        results.push_back(std::move(result));
    }
    std::ofstream file(out, std::ios::binary);
    file << recomp::EmitFile(results);
    std::cout << results.size() << " funcao(oes) traduzida(s) em " << out << "\n";
    return file ? 0 : 1;
}
