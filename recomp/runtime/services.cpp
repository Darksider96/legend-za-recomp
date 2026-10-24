// Servicos nativos do Horizon acessados por IPC
//
// sm: (gerenciador de servicos) entrega sessoes para os demais. Servicos ainda sem implementacao recebem uma
// sessao "pendente": a sessao e criada (o GetService do sdk funciona), e a primeira requisicao para o jogo com o
// nome do servico e o numero do comando — e o proximo item da lista da R5.

#include "ipc.hpp"
#include "services.hpp"

#include "recomp/runtime.hpp"

#include <functional>
#include <map>

namespace recomp::ipc {

namespace {

class PendingService final : public Service {
public:
    using Service::Service;
    void Handle(Context& c, Request& request, Response&) override {
        Fatal(c, "[ipc] %s: comando %u nao implementado (%s, %llu bytes de argumentos)", Name().c_str(), request.command, request.tipc ? "TIPC" : "CMIF",
              static_cast<unsigned long long>(request.data.size()));
    }
};

std::string ServiceName(uint64_t raw) {
    std::string name;
    for (int i = 0; i < 8; ++i) {
        const char ch = static_cast<char>((raw >> (8 * i)) & 0xFF);
        if (!ch) break;
        name += ch;
    }
    return name;
}

class ServiceManager final : public Service {
public:
    ServiceManager() : Service("sm:") {}
    void Handle(Context& c, Request& request, Response& response) override {
        switch (request.command) {
        case 0:  // RegisterClient (Initialize)
            Log("[ipc] sm: RegisterClient");
            return;
        case 1: {  // GetServiceHandle(nome de 8 bytes) -> sessao movida
            const std::string name = ServiceName(request.Pop<uint64_t>());
            auto service = MakeService(name);
            Log("[ipc] sm: GetService(\"%s\")%s", name.c_str(), service ? "" : " -> servico sem implementacao");
            if (!service) service = std::make_shared<PendingService>(name);
            response.objects.push_back(service);
            return;
        }
        case 2: case 3: case 4:  // RegisterService / UnregisterService / DetachClient
            return;
        default:
            Fatal(c, "[ipc] sm: comando %u nao implementado", request.command);
        }
    }
};

// lm (log do sistema): OpenLogger -> ILogger; Log recebe pacotes binarios (cabecalho + campos TLV). Extraimos o
// texto da mensagem (campo 2) quando presente, senao registramos o tamanho.
class Logger final : public Service {
public:
    Logger() : Service("lm:ILogger") {}
    void Handle(Context& c, Request& request, Response&) override {
        switch (request.command) {
        case 0: {  // Log(buffer A/X)
            const Buffer* b = !request.a.empty() ? &request.a[0] : !request.x.empty() ? &request.x[0] : nullptr;
            if (!b || !b->address) return;
            const auto* p = reinterpret_cast<const uint8_t*>(b->address);
            // Pacote: cabecalho de 0x18 bytes, depois campos {id u8, tamanho uleb128, dados}
            std::string text;
            for (size_t at = 0x18; at < b->size;) {
                const uint8_t id = p[at++];
                uint64_t len = 0;
                for (int shift = 0; at < b->size; shift += 7) {
                    const uint8_t byte = p[at++];
                    len |= uint64_t{byte & 0x7Fu} << shift;
                    if (!(byte & 0x80)) break;
                }
                if (at + len > b->size) break;
                if (id == 2) text.assign(reinterpret_cast<const char*>(p + at), len);
                at += len;
            }
            if (!text.empty()) Log("[lm] %s", text.c_str());
            return;
        }
        case 1:  // SetDestination
            return;
        default:
            Fatal(c, "[ipc] lm:ILogger: comando %u nao implementado", request.command);
        }
    }
};

class LogManager final : public Service {
public:
    LogManager() : Service("lm") {}
    void Handle(Context& c, Request& request, Response& response) override {
        if (request.command == 0) {  // OpenLogger(pid)
            response.objects.push_back(std::make_shared<Logger>());
            return;
        }
        Fatal(c, "[ipc] lm: comando %u nao implementado", request.command);
    }
};

using Factory = std::function<std::shared_ptr<Service>()>;

}  // namespace

void TableService::Handle(Context& c, Request& request, Response& response) {
    const auto it = handlers_.find(request.command);
    if (it == handlers_.end()) {
        Fatal(c, "[ipc] %s: comando %u nao implementado (%s, %llu bytes de argumentos)", Name().c_str(), request.command, request.tipc ? "TIPC" : "CMIF",
              static_cast<unsigned long long>(request.data.size()));
    }
    it->second(c, request, response);
}

TableService::Handler Accept() {
    return [](Context&, Request&, Response&) {};
}

namespace {

const std::map<std::string, Factory>& Registry() {
    static const std::map<std::string, Factory> registry = {
        {"sm:", [] { return std::make_shared<ServiceManager>(); }},
        {"lm", [] { return std::make_shared<LogManager>(); }},
        {"appletOE", MakeAppletOE},
        {"apm", MakeApm},
    };
    return registry;
}

}  // namespace

std::shared_ptr<Service> MakeService(const std::string& name) {
    const auto& registry = Registry();
    const auto it = registry.find(name);
    return it == registry.end() ? nullptr : it->second();
}

} // namespace recomp::ipc
