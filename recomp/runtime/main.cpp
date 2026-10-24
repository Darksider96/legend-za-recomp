// za_recomp — executa o jogo recompilado (fase R4: boot ate a primeira SVC/funcao nativa nao implementada)
//
// uso: za_recomp [pasta do exefs]   (padrao: exefs)
//   Faz o papel do rtld: mapeia main e sdk, aplica relocacoes, liga importacoes nos dois sentidos e chama
//   nn::init::Start do sdk como o rtld chama, com callbacks nativos para inicializar e finalizar os modulos
//   (DT_INIT/DT_FINI, na ordem do rtld: init do ultimo carregado para o primeiro, fini ao contrario).

#include "kernel.hpp"
#include "loader.hpp"
#include "threads.hpp"
#include "recomp/runtime.hpp"

#include <windows.h>

#include <chrono>
#include <cstdio>
#include <cstring>
#include <string>
#include <vector>

namespace recomp {

uint64_t ReadCounter() {
    const auto ns = std::chrono::steady_clock::now().time_since_epoch();
    return static_cast<uint64_t>(std::chrono::duration_cast<std::chrono::nanoseconds>(ns).count()) * 192 / 10000;  // 19,2 MHz
}

} // namespace recomp

namespace {

std::vector<recomp::LoadedModule> g_modules;

constexpr uint64_t kStackSize = 16 << 20;

void* Allocate(uint64_t size) { return VirtualAlloc(nullptr, size, MEM_RESERVE | MEM_COMMIT, PAGE_READWRITE); }

// Falhas de acesso do codigo traduzido (ponteiro do jogo para memoria nao mapeada etc.): registra e encerra
LONG WINAPI OnException(EXCEPTION_POINTERS* info) {
    const EXCEPTION_RECORD& r = *info->ExceptionRecord;
    if (r.ExceptionCode != EXCEPTION_ACCESS_VIOLATION && r.ExceptionCode != EXCEPTION_STACK_OVERFLOW && r.ExceptionCode != EXCEPTION_ILLEGAL_INSTRUCTION &&
        r.ExceptionCode != EXCEPTION_INT_DIVIDE_BY_ZERO) {
        return EXCEPTION_CONTINUE_SEARCH;
    }
    const unsigned long long target = r.NumberParameters >= 2 ? r.ExceptionInformation[1] : 0;
    const char* kind = r.NumberParameters >= 1 ? (r.ExceptionInformation[0] == 0 ? "leitura" : r.ExceptionInformation[0] == 1 ? "escrita" : "execucao") : "?";
    recomp::Log("[host] excecao 0x%08lx (%s em 0x%llx) no host RIP=0x%llx; alvo = %s", r.ExceptionCode, kind, target,
                static_cast<unsigned long long>(info->ContextRecord->Rip), recomp::DescribeAddress(target).c_str());
    std::fflush(nullptr);
    TerminateProcess(GetCurrentProcess(), 4);
    return EXCEPTION_CONTINUE_SEARCH;
}

// Callbacks que o rtld passa para nn::init::Start
void RtldReady(recomp::Context&) { recomp::Log("[rtld] pronto"); }

void RtldInitModules(recomp::Context& c) {
    for (auto it = g_modules.rbegin(); it != g_modules.rend(); ++it) {
        if (!it->nso.Init()) continue;
        recomp::Log("[rtld] DT_INIT de %s em %s", it->name.c_str(), recomp::DescribeAddress(it->base + it->nso.Init()).c_str());
        recomp::Call(c, it->base + it->nso.Init());
    }
}

void RtldFiniModules(recomp::Context& c) {
    for (const recomp::LoadedModule& m : g_modules) {
        if (m.nso.Fini()) recomp::Call(c, m.base + m.nso.Fini());
    }
}

}  // namespace

int main(int argc, char** argv) {
    const std::string exefs = argc > 1 ? argv[1] : "exefs";
    AddVectoredExceptionHandler(1, OnException);
    g_modules.resize(2);
    g_modules[0].name = "main";
    g_modules[0].base = recomp::kImageBase;
    g_modules[1].name = "sdk";
    g_modules[1].base = recomp::kSdkBase;
    std::string error;
    for (recomp::LoadedModule& m : g_modules) {
        if (!m.nso.Load(exefs + "/" + m.name, error) || !recomp::MapModule(m, error)) {
            std::fprintf(stderr, "%s\n", error.c_str());
            return 2;
        }
    }
    recomp::SetDescribedModules(&g_modules);
    // ProgramId do main.npdm: cabecalho META (offset do ACI0 em 0x70), ACI0 com o id em 0x10
    uint64_t program_id = 0;
    if (FILE* npdm = std::fopen((exefs + "/main.npdm").c_str(), "rb")) {
        uint8_t meta[0x80] = {}, aci0[0x18] = {};
        uint32_t aci0_offset = 0;
        if (std::fread(meta, 1, sizeof(meta), npdm) == sizeof(meta) && std::memcmp(meta, "META", 4) == 0) {
            std::memcpy(&aci0_offset, meta + 0x70, 4);
            if (std::fseek(npdm, aci0_offset, SEEK_SET) == 0 && std::fread(aci0, 1, sizeof(aci0), npdm) == sizeof(aci0) && std::memcmp(aci0, "ACI0", 4) == 0) {
                std::memcpy(&program_id, aci0 + 0x10, 8);
            }
        }
        std::fclose(npdm);
    }
    recomp::SetKernelProcessInfo(program_id, g_modules[0].nso.Image().size() + g_modules[1].nso.Image().size());
    recomp::Log("[loader] ProgramId %016llx", static_cast<unsigned long long>(program_id));
    recomp::LinkStats stats;
    if (!recomp::LinkModules(g_modules, {}, stats, error)) {
        std::fprintf(stderr, "%s\n", error.c_str());
        return 2;
    }
    recomp::Log("[loader] main em 0x%llx, sdk em 0x%llx; %llu relocacoes, %llu importacoes ligadas entre modulos, %llu nativas, %llu sem definicao",
                static_cast<unsigned long long>(recomp::kImageBase), static_cast<unsigned long long>(recomp::kSdkBase),
                static_cast<unsigned long long>(stats.relocations), static_cast<unsigned long long>(stats.imports_from_modules),
                static_cast<unsigned long long>(stats.imports_to_host), static_cast<unsigned long long>(stats.unresolved));

    const recomp::Symbol* start = g_modules[1].nso.FindExport("_ZN2nn4init5StartEmmPFvvES2_S2_");
    if (!start) {
        std::fprintf(stderr, "sdk sem nn::init::Start\n");
        return 2;
    }
    recomp::Context c;
    auto* stack = static_cast<uint8_t*>(Allocate(kStackSize));
    auto* tls = static_cast<uint8_t*>(Allocate(0x1000));     // ThreadLocalRegion (TPIDRRO_EL0): buffer de IPC + dados do SO
    auto* rtld_area = static_cast<uint8_t*>(Allocate(0x1000));  // o rtld passa o fim da propria imagem em x1
    c.sp = (reinterpret_cast<uint64_t>(stack) + kStackSize - 0x100) & ~uint64_t{0xF};
    c.tpidrro = reinterpret_cast<uint64_t>(tls);
    c.x[0] = recomp::kernel::CreateMainThread(c);
    c.x[1] = reinterpret_cast<uint64_t>(rtld_area);
    c.x[2] = recomp::RegisterHost("rtld:pronto", RtldReady);
    c.x[3] = recomp::RegisterHost("rtld:init", RtldInitModules);
    c.x[4] = recomp::RegisterHost("rtld:fini", RtldFiniModules);
    const uint64_t entry = recomp::kSdkBase + start->value;
    recomp::Log("[boot] nn::init::Start em %s", recomp::DescribeAddress(entry).c_str());
    recomp::Call(c, entry);
    recomp::Log("[boot] nn::init::Start retornou");
    return 0;
}
