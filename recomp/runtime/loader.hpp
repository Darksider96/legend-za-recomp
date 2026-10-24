#pragma once
// Carregador: mapeia os modulos NSO nos enderecos fixos, aplica relocacoes e liga importacoes (papel do rtld)

#include "recomp/nso.hpp"
#include "recomp/runtime.hpp"

#include <map>
#include <memory>
#include <string>
#include <vector>

namespace recomp {

struct LoadedModule {
    std::string name;
    uint64_t base{0};
    NsoModule nso;
};

struct LinkStats {
    size_t relocations{0}, imports_from_modules{0}, imports_to_host{0}, unresolved{0};
};

// Mapeia a imagem do modulo em base (VirtualAlloc no endereco fixo) e copia os segmentos
bool MapModule(LoadedModule& module, std::string& error);

// Aplica as relocacoes de todos os modulos. Simbolos sao procurados como no rtld: primeiro as funcoes nativas
// registradas por nome (overrides), depois as exportacoes de cada modulo na ordem de carga. Importacoes sem
// definicao viram funcoes nativas que param o jogo com o nome quando chamadas.
bool LinkModules(std::vector<LoadedModule>& modules, const std::map<std::string, HostFunction>& overrides, LinkStats& stats, std::string& error);

// Registra os modulos para DescribeAddress
void SetDescribedModules(const std::vector<LoadedModule>* modules);

} // namespace recomp
