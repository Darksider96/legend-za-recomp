// a64_recomp — traduz funcoes AArch64 do main para C++
//
// uso: a64_recomp <main> --function <rva> [--function <rva>...] --out <arquivo.cpp>
//      a64_recomp <main> --stats
//      a64_recomp <nso> --all --out-dir <pasta> [--extra <funcoes.csv>] [--files N] [--base B --prefix P --table T]
//   --stats traduz todas as funcoes do .eh_frame em memoria e informa quantas saem sem instrucoes nao suportadas.
//   --all traduz o modulo inteiro: funcoes do .eh_frame, as de --extra (CSV rva,tamanho[,...], p.ex. as propostas
//   para as lacunas) e os stubs da PLT; grava exatamente N arquivos guest_NNN.cpp (padrao 128), missing.cpp (alvos que
//   sao chamados mas nao sao inicio de funcao) e dispatch_table.cpp (endereco -> funcao, ordenado, para o Dispatch).
//   --base/--prefix/--table: outro modulo (p.ex. o sdk) em outra base, com outro prefixo de nome e outra tabela
//   (padrao: main em 0x7100000000, "f_", kMainFunctions).
//   O codigo gerado contem codigo do jogo traduzido: gravar apenas fora do repositorio (build/).

#include "recomp/context.hpp"
#include "recomp/emit_c.hpp"
#include "recomp/nso.hpp"

#include <algorithm>
#include <cstring>
#include <filesystem>
#include <format>
#include <fstream>
#include <iostream>
#include <map>
#include <set>
#include <sstream>
#include <string>

namespace {

// Stubs da PLT: adrp x16, pagina / ldr x17, [x16, #off] / add x16, x16, #off / br x17
std::vector<recomp::FunctionRange> FindPltStubs(const recomp::NsoModule& module) {
    std::vector<recomp::FunctionRange> stubs;
    const auto image = module.Image();
    uint32_t w[4];
    for (uint64_t pc = 0; pc + 16 <= module.Text().size; pc += 4) {
        std::memcpy(w, image.data() + pc, 16);
        if ((w[0] & 0x9F00001F) == 0x90000010 && (w[1] & 0xFFC003FF) == 0xF9400211 && (w[2] & 0xFFC003FF) == 0x91000210 && w[3] == 0xD61F0220) {
            stubs.push_back({pc, 16});
        }
    }
    return stubs;
}

std::vector<recomp::FunctionRange> ReadExtra(const std::string& path) {
    std::vector<recomp::FunctionRange> out;
    std::ifstream file(path);
    std::string line;
    std::getline(file, line);  // cabecalho
    while (std::getline(file, line)) {
        std::stringstream fields(line);
        std::string rva, size;
        if (std::getline(fields, rva, ',') && std::getline(fields, size, ',')) out.push_back({std::stoull(rva, nullptr, 0), std::stoull(size, nullptr, 0)});
    }
    return out;
}

int TranslateAll(const recomp::NsoModule& module, const std::string& out_dir, const std::string& extra_path, size_t file_count, uint64_t base,
                 const std::string& table_name) {
    // Conjunto de funcoes: o .eh_frame tem prioridade; stubs e extras entram onde nao sobrepoem
    std::vector<recomp::FunctionRange> all(module.Functions().begin(), module.Functions().end());
    const size_t eh = all.size();
    std::vector<recomp::FunctionRange> candidates = FindPltStubs(module);
    const size_t plt = candidates.size();
    if (!extra_path.empty()) {
        const auto extra = ReadExtra(extra_path);
        candidates.insert(candidates.end(), extra.begin(), extra.end());
    }
    const std::vector<recomp::FunctionRange>& known = module.Functions();
    size_t added = 0;
    for (const recomp::FunctionRange& f : candidates) {
        const auto it = std::lower_bound(known.begin(), known.end(), f.start, [](const recomp::FunctionRange& a, uint64_t v) { return a.start + a.size <= v; });
        if (it != known.end() && it->start < f.start + f.size) continue;
        all.push_back(f);
        ++added;
    }
    std::sort(all.begin(), all.end(), [](const auto& a, const auto& b) { return a.start < b.start; });
    all.erase(std::unique(all.begin(), all.end(), [](const auto& a, const auto& b) { return a.start == b.start; }), all.end());

    std::filesystem::create_directories(out_dir);
    const size_t per_file = (all.size() + file_count - 1) / file_count;
    std::set<uint64_t> defined, called;
    for (const auto& f : all) defined.insert(f.start);
    size_t unsupported = 0, bytes = 0, files = 0;
    std::vector<recomp::EmitResult> batch;
    // Grava so se mudou: o build incremental nao recompila arquivos identicos
    const auto write = [&](const std::string& name, const std::string& code) {
        const auto file = std::filesystem::path(out_dir) / name;
        std::ifstream old_file(file, std::ios::binary);
        const std::string old_code((std::istreambuf_iterator<char>(old_file)), std::istreambuf_iterator<char>());
        old_file.close();
        if (old_code != code) std::ofstream(file, std::ios::binary) << code;
    };
    const auto flush = [&] {
        if (batch.empty()) return;
        const std::string code = recomp::EmitFile(batch);
        bytes += code.size();
        write(std::format("guest_{:03}.cpp", files++), code);
        batch.clear();
    };
    for (const recomp::FunctionRange& f : all) {
        recomp::EmitResult result;
        if (!recomp::EmitFunction(module, f, result)) continue;
        unsupported += result.unsupported.size();
        called.insert(result.callees.begin(), result.callees.end());
        batch.push_back(std::move(result));
        if (batch.size() >= per_file) flush();
    }
    flush();
    while (files < file_count) write(std::format("guest_{:03}.cpp", files++), "// vazio\n");

    // Alvos chamados que nao sao inicio de funcao: o runtime informa e para
    std::string missing = "// Gerado por recomp/tools/a64_recomp — nao versionar\n#include \"recomp/runtime.hpp\"\n\nnamespace guest {\n";
    size_t missing_count = 0;
    for (uint64_t rva : called) {
        if (defined.contains(rva)) continue;
        missing += std::format("void {}(recomp::Context& c) {{ recomp::MissingFunction(c, 0x{:x}); }}\n", recomp::FunctionName(rva), base + rva);
        ++missing_count;
    }
    missing += "} // namespace guest\n";
    write("missing.cpp", missing);

    std::string table = "// Gerado por recomp/tools/a64_recomp — nao versionar\n#include \"recomp/runtime.hpp\"\n\nnamespace guest {\n";
    for (uint64_t rva : defined) table += std::format("void {}(recomp::Context& c);\n", recomp::FunctionName(rva));
    table += std::format("}} // namespace guest\n\nnamespace recomp {{\nextern const GuestFunction {0}[];\nconst GuestFunction {0}[] = {{\n", table_name);
    for (uint64_t rva : defined) table += std::format("    {{0x{:x}, guest::{}}},\n", base + rva, recomp::FunctionName(rva));
    table += std::format("}};\nextern const size_t {0}Count;\nconst size_t {0}Count = sizeof({0}) / sizeof({0}[0]);\n}} // namespace recomp\n", table_name);
    write("dispatch_table.cpp", table);

    std::cout << all.size() << " funcoes (" << eh << " do .eh_frame, " << plt << " stubs da PLT encontrados, " << added << " stubs/extras adicionados)\n"
              << "  " << files << " arquivos, " << bytes / (1024 * 1024) << " MiB de C++; " << unsupported << " instrucoes nao suportadas; "
              << missing_count << " alvos sem funcao (missing.cpp)\n";
    return 0;
}

}  // namespace

int main(int argc, char** argv) {
    std::string path, out, out_dir, extra, prefix = "f_", table_name = "kMainFunctions";
    uint64_t base = recomp::kImageBase;
    std::vector<uint64_t> wanted;
    bool stats = false, all = false;
    size_t file_count = 128;
    for (int i = 1; i < argc; ++i) {
        const std::string arg = argv[i];
        if (arg == "--function" && i + 1 < argc) wanted.push_back(std::stoull(argv[++i], nullptr, 0));
        else if (arg == "--out" && i + 1 < argc) out = argv[++i];
        else if (arg == "--out-dir" && i + 1 < argc) out_dir = argv[++i];
        else if (arg == "--extra" && i + 1 < argc) extra = argv[++i];
        else if (arg == "--files" && i + 1 < argc) file_count = std::stoull(argv[++i]);
        else if (arg == "--base" && i + 1 < argc) base = std::stoull(argv[++i], nullptr, 0);
        else if (arg == "--prefix" && i + 1 < argc) prefix = argv[++i];
        else if (arg == "--table" && i + 1 < argc) table_name = argv[++i];
        else if (arg == "--stats") stats = true;
        else if (arg == "--all") all = true;
        else path = arg;
    }
    recomp::NsoModule module;
    std::string error;
    if (all) {
        if (path.empty() || out_dir.empty() || !module.Load(path, error)) {
            std::cerr << (error.empty() ? "uso: a64_recomp <main> --all --out-dir <pasta> [--extra <csv>] [--files N]" : error) << "\n";
            return 2;
        }
        recomp::SetModule(base, prefix);
        return TranslateAll(module, out_dir, extra, file_count, base, table_name);
    }
    if (path.empty() || (!stats && (wanted.empty() || out.empty())) || !module.Load(path, error)) {
        std::cerr << (error.empty() ? "uso: a64_recomp <main> --function <rva>... --out <arquivo.cpp> | --stats" : error) << "\n";
        return 2;
    }
    const auto& functions = module.Functions();

    if (stats) {
        size_t clean = 0, instructions = 0, unsupported = 0, clean_instructions = 0, tables = 0, table_targets = 0;
        std::map<std::string, size_t> blockers;  // operacao nao suportada -> funcoes bloqueadas por ela
        std::ofstream table_csv;
        if (!out.empty()) {  // --stats --out <csv>: destinos das tabelas de salto (br_rva,alvo_rva) para conferencia
            table_csv.open(out);
            table_csv << "br_rva,alvo_rva\n";
        }
        for (const recomp::FunctionRange& f : functions) {
            recomp::EmitResult result;
            if (!recomp::EmitFunction(module, f, result)) continue;
            for (const auto& [br, targets] : result.jump_tables) {
                ++tables;
                table_targets += targets.size();
                if (table_csv) {
                    for (uint64_t t : targets) table_csv << std::format("0x{:x},0x{:x}\n", br, t);
                }
            }
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
                  << "  tabelas de salto resolvidas: " << tables << " (" << table_targets << " destinos distintos)\n"
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
