#pragma once
// IPC do Horizon (HIPC + CMIF/TIPC) emulado no runtime: sessoes, dominios e servicos nativos
//
// O sdk recompilado monta as mensagens no buffer de IPC da thread (TLS, TPIDRRO_EL0 + 0, 0x100 bytes) e chama
// SendSyncRequest; aqui a mensagem e lida, entregue ao servico nativo da sessao e a resposta e escrita no mesmo
// buffer, no formato que o cliente do sdk espera.

#include "recomp/context.hpp"

#include <cstdint>
#include <cstring>
#include <functional>
#include <map>
#include <memory>
#include <string>
#include <vector>

namespace recomp::ipc {

struct Buffer {
    uint64_t address{0}, size{0};
    uint32_t attribute{0};  // A/B/W: modo; X: contador
};

struct Request {
    uint32_t type{0};          // 4 Request, 5 Control, 2 Close, 6/7 com contexto; >= 16 TIPC (comando = type - 16)
    uint32_t command{0};
    bool tipc{false};
    bool has_pid{false};
    uint64_t pid{0};
    std::vector<uint32_t> copy_handles, move_handles;
    std::vector<Buffer> x, a, b, w, c;
    std::vector<uint8_t> data;          // argumentos (sem cabecalhos CMIF)
    std::vector<uint32_t> in_objects;   // dominio: ids de objetos de entrada
    size_t cursor{0};

    template <typename T>
    T Pop() {
        T value{};
        cursor = (cursor + alignof(T) - 1) & ~(alignof(T) - 1);
        if (cursor + sizeof(T) <= data.size()) std::memcpy(&value, data.data() + cursor, sizeof(T));
        cursor += sizeof(T);
        return value;
    }
};

class Service;

struct Response {
    uint32_t result{0};
    std::vector<uint8_t> data;
    std::vector<uint32_t> copy_handles, move_handles;
    std::vector<std::shared_ptr<Service>> objects;  // novos objetos: handle movido (sessao) ou id de dominio

    template <typename T>
    void Push(const T& value) {
        const size_t at = (data.size() + alignof(T) - 1) & ~(alignof(T) - 1);
        data.resize(at + sizeof(T));
        std::memcpy(data.data() + at, &value, sizeof(T));
    }
};

// Servico nativo: recebe a requisicao ja decodificada e preenche a resposta
class Service {
public:
    explicit Service(std::string name) : name_(std::move(name)) {}
    virtual ~Service() = default;
    const std::string& Name() const { return name_; }
    virtual void Handle(Context& c, Request& request, Response& response) = 0;
    // Tamanho do buffer de ponteiros (comando de controle QueryPointerBufferSize)
    virtual uint16_t PointerBufferSize() const { return 0x8000; }

private:
    std::string name_;
};

// Servico descrito por uma tabela comando -> tratador; comando fora da tabela para o jogo com nome e numero
class TableService : public Service {
public:
    using Handler = std::function<void(Context&, Request&, Response&)>;
    TableService(std::string name, std::map<uint32_t, Handler> handlers) : Service(std::move(name)), handlers_(std::move(handlers)) {}
    void Handle(Context& c, Request& request, Response& response) override;

private:
    std::map<uint32_t, Handler> handlers_;
};

// Tratador que so aceita o comando (sem saidas)
TableService::Handler Accept();

// Cria (ou devolve) o servico nativo registrado com esse nome no sm; nullptr se nao existe
std::shared_ptr<Service> MakeService(const std::string& name);

// Registra um objeto novo na tabela de handles do processo e devolve o handle
uint32_t CreateSessionHandle(std::shared_ptr<Service> service);

// SVCs
void ConnectToNamedPort(Context& c);
void SendSyncRequest(Context& c);
bool CloseHandle(uint32_t handle);

} // namespace recomp::ipc
