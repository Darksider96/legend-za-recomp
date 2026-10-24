// a64_dump — decodifica as instrucoes das funcoes do .eh_frame e grava CSV para validar o decodificador
// uso: a64_dump <main> [--stride N] [--out arquivo.csv]
//   stride N: uma a cada N instrucoes (padrao 13). Saida derivada do jogo: gravar fora do repositorio.

#include "recomp/a64.hpp"
#include "recomp/nso.hpp"

#include <cstring>
#include <fstream>
#include <iostream>
#include <map>
#include <string>

int main(int argc, char** argv) {
    std::string path, out = "a64_dump.csv";
    uint64_t stride = 13;
    for (int i = 1; i < argc; ++i) {
        const std::string arg = argv[i];
        if (arg == "--stride" && i + 1 < argc) stride = std::stoull(argv[++i]);
        else if (arg == "--out" && i + 1 < argc) out = argv[++i];
        else path = arg;
    }
    recomp::NsoModule module;
    std::string error;
    if (path.empty() || !module.Load(path, error)) {
        std::cerr << (path.empty() ? "uso: a64_dump <main> [--stride N] [--out arquivo.csv]" : error) << "\n";
        return 1;
    }
    std::ofstream csv(out);
    csv << "rva,word,op,sf,s,rd,rn,rm,ra,imm,target,size,sign,dest64,mode,shift,amount,extend,cond,immr,imms,bit,sysreg\n";
    std::map<recomp::a64::Op, uint64_t> histogram;
    uint64_t total = 0, index = 0;
    const auto image = module.Image();
    for (const recomp::FunctionRange& f : module.Functions()) {
        for (uint64_t pc = f.start; pc + 4 <= f.start + f.size; pc += 4, ++index) {
            uint32_t word = 0;
            std::memcpy(&word, image.data() + pc, 4);
            const recomp::a64::Insn d = recomp::a64::Decode(word, pc);
            ++histogram[d.op];
            ++total;
            if (index % stride) continue;
            csv << std::hex << pc << "," << word << std::dec << "," << recomp::a64::OpName(d.op) << "," << d.sf << "," << d.set_flags << ","
                << int{d.rd} << "," << int{d.rn} << "," << int{d.rm} << "," << int{d.ra} << "," << d.imm << "," << std::hex << d.target << std::dec
                << "," << int{d.size} << "," << d.sign_extend << "," << d.dest64 << "," << static_cast<int>(d.mode) << "," << int{d.shift} << ","
                << int{d.amount} << "," << int{d.extend} << "," << int{d.cond} << "," << int{d.immr} << "," << int{d.imms} << "," << int{d.bit}
                << "," << d.sysreg << "\n";
        }
    }
    std::cout << total << " instrucoes; nao decodificadas: " << histogram[recomp::a64::Op::Unknown] << " ("
              << 100.0 * static_cast<double>(histogram[recomp::a64::Op::Unknown]) / static_cast<double>(total) << "%)\n";
    return 0;
}
