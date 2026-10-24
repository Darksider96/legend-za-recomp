// a64_insn_tests — gera os testes diferenciais por instrucao (fase R3)
//
// uso: a64_insn_tests <main> --out-dir <pasta> [--per-shape K]
//   Agrupa as instrucoes decodificadas do .eh_frame por forma (operacao, largura, flags, modo de enderecamento,
//   registrador 31...) e sorteia ate K palavras reais do jogo por forma (semente fixa). Cada uma vira uma funcao
//   t_<rva> de uma instrucao so; as saidas (fall-through, saltos, chamadas) viram stubs que registram o destino.
//   Gera em <pasta>: insn_tests.cpp (codigo traduzido), insn_tests.csv (metadados para o harness) e image.bin
//   (imagem plana do main, para o emulador de referencia). Tudo derivado do jogo: so dentro de build/.

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
#include <string>
#include <vector>

namespace {

using recomp::a64::AddrMode;
using recomp::a64::Insn;
using recomp::a64::Op;

bool AccessesMemory(Op op) {
    switch (op) {
    case Op::Load: case Op::Store: case Op::LoadPair: case Op::StorePair: case Op::Prefetch:
    case Op::LoadExclusive: case Op::StoreExclusive: case Op::LoadExclusivePair: case Op::StoreExclusivePair:
    case Op::LoadAcquire: case Op::StoreRelease:
    case Op::LoadStruct: case Op::StoreStruct: case Op::LoadLane: case Op::StoreLane: case Op::LoadReplicate:
        return true;
    default:
        return false;
    }
}

bool IsBranch(Op op) {
    return op == Op::B || op == Op::Bl || op == Op::BCond || op == Op::Cbz || op == Op::Cbnz || op == Op::Tbz || op == Op::Tbnz;
}

// Instrucoes fora do teste: nao decodificadas, excecoes e leituras dependentes do tempo
bool Testable(const Insn& d) {
    if (d.op == Op::Unknown || d.op == Op::Svc || d.op == Op::Brk || d.op == Op::Udf) return false;
    if (d.op == Op::Mrs && d.sysreg == recomp::a64::kSysCntvctEl0) return false;
    if (IsBranch(d.op) && d.target == d.pc) return false;  // salto para si mesmo: laco infinito com uma instrucao
    if (AccessesMemory(d.op) && d.mode == AddrMode::Register && d.rn == d.rm) return false;
    return true;
}

std::string Shape(const Insn& d) {
    return std::format("{}/sf{}/s{}/z{}/sx{}/d{}/m{}/sh{}/a{}/e{}/o{}/r{}{}{}{}", recomp::a64::OpName(d.op), int{d.sf}, int{d.set_flags}, int{d.size},
                       int{d.sign_extend}, int{d.dest64}, static_cast<int>(d.mode), int{d.shift}, d.amount != 0, int{d.extend}, int{d.ordered},
                       d.rd == 31, d.rn == 31, d.rm == 31, d.ra == 31);
}

}  // namespace

int main(int argc, char** argv) {
    std::string path, out_dir;
    size_t per_shape = 3;
    for (int i = 1; i < argc; ++i) {
        const std::string arg = argv[i];
        if (arg == "--out-dir" && i + 1 < argc) out_dir = argv[++i];
        else if (arg == "--per-shape" && i + 1 < argc) per_shape = std::stoull(argv[++i]);
        else path = arg;
    }
    recomp::NsoModule module;
    std::string error;
    if (path.empty() || out_dir.empty() || !module.Load(path, error)) {
        std::cerr << (error.empty() ? "uso: a64_insn_tests <main> --out-dir <pasta> [--per-shape K]" : error) << "\n";
        return 2;
    }
    std::filesystem::create_directories(out_dir);

    // Amostragem por reservatorio, deterministica, para espalhar a escolha pelo binario inteiro
    struct Bucket {
        uint64_t seen{0};
        std::vector<Insn> picked;
    };
    std::map<std::string, Bucket> buckets;
    uint64_t rng = 0x9E3779B97F4A7C15ull;
    const auto next = [&rng] {
        rng ^= rng << 13;
        rng ^= rng >> 7;
        rng ^= rng << 17;
        return rng;
    };
    const auto image = module.Image();
    for (const recomp::FunctionRange& f : module.Functions()) {
        for (uint64_t pc = f.start; pc + 4 <= f.start + f.size; pc += 4) {
            uint32_t word = 0;
            std::memcpy(&word, image.data() + pc, 4);
            const Insn d = recomp::a64::Decode(word, pc);
            if (!Testable(d)) continue;
            Bucket& bucket = buckets[Shape(d)];
            ++bucket.seen;
            if (bucket.picked.size() < per_shape) {
                bucket.picked.push_back(d);
            } else {
                const uint64_t slot = next() % bucket.seen;
                if (slot < per_shape) bucket.picked[slot] = d;
            }
        }
    }

    std::vector<std::pair<Insn, std::string>> selected;
    for (const auto& [shape, bucket] : buckets) {
        for (const Insn& d : bucket.picked) selected.emplace_back(d, shape);
    }
    std::sort(selected.begin(), selected.end(), [](const auto& a, const auto& b) { return a.first.pc < b.first.pc; });

    std::vector<recomp::EmitResult> results;
    std::set<uint64_t> callees;
    std::ofstream csv(std::filesystem::path(out_dir) / "insn_tests.csv");
    csv << "rva,word,op,shape,rd,rn,rm,ra,mode,memory,size,target\n";
    std::string table;
    for (const auto& [d, shape] : selected) {
        recomp::EmitResult result;
        if (!recomp::EmitFunction(module, {d.pc, 4}, result, std::format("t_{:x}", d.pc)) || !result.unsupported.empty()) continue;
        callees.insert(result.callees.begin(), result.callees.end());
        csv << std::format("{:x},{:08x},{},{},{},{},{},{},{},{},{},{:x}\n", d.pc, d.word, recomp::a64::OpName(d.op), shape, d.rd, d.rn, d.rm, d.ra,
                           static_cast<int>(d.mode), AccessesMemory(d.op) ? 1 : 0, d.size, d.target);
        table += std::format("    {{0x{:x}, t_{:x}}},\n", d.pc, d.pc);
        results.push_back(std::move(result));
    }

    std::string file = "// Gerado por recomp/tools/a64_insn_tests a partir do dump do usuario — nao versionar\n"
                       "#include \"recomp/simd.hpp\"\n#include \"insn_difftest.hpp\"\n\n#include <bit>\n\nnamespace guest {\n\n";
    // Stubs: toda saida da instrucao (fall-through, salto, chamada) registra o destino e volta
    for (uint64_t callee : callees) {
        file += std::format("void {}(recomp::Context&) {{ recomp::test::Exit(recomp::kImageBase + 0x{:x}ull); }}\n", recomp::FunctionName(callee), callee);
    }
    file += "\n";
    for (const recomp::EmitResult& r : results) file += r.code + "\n";
    file += "} // namespace guest\n\nnamespace recomp::test {\nusing namespace guest;\nconst InsnTest kInsnTests[] = {\n" + table +
            "};\nconst size_t kInsnTestCount = sizeof(kInsnTests) / sizeof(kInsnTests[0]);\n} // namespace recomp::test\n";
    std::ofstream(std::filesystem::path(out_dir) / "insn_tests.cpp", std::ios::binary) << file;

    std::ofstream bin(std::filesystem::path(out_dir) / "image.bin", std::ios::binary);
    bin.write(reinterpret_cast<const char*>(image.data()), static_cast<std::streamsize>(image.size()));
    std::cout << buckets.size() << " formas, " << results.size() << " testes em " << out_dir << "\n";
    return bin ? 0 : 1;
}
