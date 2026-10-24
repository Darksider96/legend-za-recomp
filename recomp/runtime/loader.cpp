#include "loader.hpp"

#include <windows.h>

#include <cstring>
#include <format>

namespace recomp {

namespace {

const std::vector<LoadedModule>* g_described = nullptr;

void Write64(uint64_t address, uint64_t value) { std::memcpy(reinterpret_cast<void*>(address), &value, 8); }

}  // namespace

bool MapModule(LoadedModule& module, std::string& error) {
    const auto image = module.nso.Image();
    void* p = VirtualAlloc(reinterpret_cast<void*>(module.base), image.size(), MEM_RESERVE | MEM_COMMIT, PAGE_READWRITE);
    if (p != reinterpret_cast<void*>(module.base)) {
        error = std::format("{}: nao foi possivel mapear {} bytes em 0x{:x}", module.name, image.size(), module.base);
        return false;
    }
    std::memcpy(p, image.data(), image.size());
    return true;
}

bool LinkModules(std::vector<LoadedModule>& modules, const std::map<std::string, HostFunction>& overrides, LinkStats& stats, std::string& error) {
    std::map<std::string, uint64_t> host_addresses;  // nome -> endereco falso (registra uma vez)
    const auto host = [&](const std::string& name, HostFunction fn) {
        auto it = host_addresses.find(name);
        if (it == host_addresses.end()) it = host_addresses.emplace(name, RegisterHost(name, fn)).first;
        return it->second;
    };
    // Importacao sem definicao: para o jogo com o nome quando for chamada (o nome fica no registro da funcao)
    const HostFunction unresolved = [](Context& c) { Fatal(c, "chamada a importacao sem definicao"); };

    const auto resolve = [&](const std::string& name, bool& found) -> uint64_t {
        found = true;
        if (const auto it = overrides.find(name); it != overrides.end()) {
            ++stats.imports_to_host;
            return host(name, it->second);
        }
        for (const LoadedModule& m : modules) {
            if (const Symbol* s = m.nso.FindExport(name)) {
                ++stats.imports_from_modules;
                return m.base + s->value;
            }
        }
        found = false;
        ++stats.unresolved;
        return host("sem definicao: " + name, unresolved);
    };

    for (LoadedModule& m : modules) {
        const auto& symbols = m.nso.Symbols();
        for (const Relocation& r : m.nso.Relocations()) {
            const uint64_t slot = m.base + r.offset;
            ++stats.relocations;
            switch (r.type) {
            case kRelocRelative:
                Write64(slot, m.base + static_cast<uint64_t>(r.addend));
                break;
            case kRelocAbs64:
            case kRelocGlobDat:
            case kRelocJumpSlot: {
                if (r.symbol >= symbols.size()) {
                    error = std::format("{}: relocacao em 0x{:x} com simbolo invalido", m.name, r.offset);
                    return false;
                }
                const Symbol& s = symbols[r.symbol];
                bool found = true;
                // Definicoes globais sao resolvidas pela busca global (como no rtld, o main pode sobrepor o sdk)
                const uint64_t value = s.name.empty() ? m.base + s.value : resolve(s.name, found);
                Write64(slot, value + (r.type == kRelocJumpSlot ? 0 : static_cast<uint64_t>(r.addend)));
                break;
            }
            default:
                error = std::format("{}: tipo de relocacao {} nao suportado em 0x{:x}", m.name, r.type, r.offset);
                return false;
            }
        }
    }
    return true;
}

void SetDescribedModules(const std::vector<LoadedModule>* modules) { g_described = modules; }

// Usado por DescribeAddress (dispatch.cpp): modulo e RVA de um endereco
bool FindModule(uint64_t address, std::string& name, uint64_t& rva) {
    if (!g_described) return false;
    for (const LoadedModule& m : *g_described) {
        if (address >= m.base && address < m.base + m.nso.Image().size()) {
            name = m.name;
            rva = address - m.base;
            return true;
        }
    }
    return false;
}

} // namespace recomp
