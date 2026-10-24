// HIPC/CMIF/TIPC: leitura das requisicoes no TLS, sessoes e dominios, escrita das respostas
//
// Layout (libnx: hipc.h, cmif.h): palavra 0 = tipo | nX<<16 | nA<<20 | nB<<24 | nW<<28; palavra 1 = nDados | flagsC<<10
// | handles<<31; descritor de handles (pid, copias, movidos); X (2 palavras), A/B/W (3); dados; C. No CMIF os dados
// comecam alinhados a 16 bytes a partir do inicio do buffer, com SFCI/SFCO (e o cabecalho de dominio antes, se for
// o caso); no TIPC (tipo >= 16) os argumentos vem direto, sem cabecalho.

#include "ipc.hpp"

#include "handles.hpp"

#include "recomp/runtime.hpp"

#include <map>
#include <mutex>

namespace recomp::ipc {

namespace {

constexpr uint32_t kSfci = 0x49434653, kSfco = 0x4F434653;
constexpr uint32_t kResultNotFound = 0xE401;           // sm: servico nao registrado (21-7)
constexpr uint32_t kResultInvalidHandle = 0xE401;      // kernel: handle invalido (1-114)
constexpr uint32_t kResultUnknownCommand = 0x1A60A;    // cmif: comando desconhecido (10-...)

struct Session final : kernel::Object {
    std::mutex mutex;  // o Horizon serializa as requisicoes de uma sessao
    std::shared_ptr<Service> service;
    bool domain{false};
    std::map<uint32_t, std::shared_ptr<Service>> objects;  // dominio: id -> objeto
    uint32_t next_object{1};
};


uint32_t Word(const uint8_t* base, size_t index) {
    uint32_t w;
    std::memcpy(&w, base + index * 4, 4);
    return w;
}

void PutWord(uint8_t* base, size_t index, uint32_t value) { std::memcpy(base + index * 4, &value, 4); }

uint64_t BufferAddress(uint32_t lo, uint32_t hi_bits) { return uint64_t{lo} | (uint64_t{hi_bits} << 32); }

// Le a mensagem do TLS. Para CMIF, separa o cabecalho de dominio e o SFCI dos argumentos.
bool Parse(const uint8_t* tls, bool domain, Request& r, uint32_t& domain_object, uint32_t& domain_command, std::string& error) {
    const uint32_t w0 = Word(tls, 0), w1 = Word(tls, 1);
    r.type = w0 & 0xFFFF;
    const unsigned nx = (w0 >> 16) & 0xF, na = (w0 >> 20) & 0xF, nb = (w0 >> 24) & 0xF, nw = (w0 >> 28) & 0xF;
    const unsigned data_words = w1 & 0x3FF, c_flags = (w1 >> 10) & 0xF;
    size_t at = 2;
    if (w1 >> 31) {
        const uint32_t hd = Word(tls, at++);
        r.has_pid = hd & 1;
        const unsigned copies = (hd >> 1) & 0xF, moves = (hd >> 5) & 0xF;
        if (r.has_pid) {
            r.pid = uint64_t{Word(tls, at)} | (uint64_t{Word(tls, at + 1)} << 32);
            at += 2;
        }
        for (unsigned i = 0; i < copies; ++i) r.copy_handles.push_back(Word(tls, at++));
        for (unsigned i = 0; i < moves; ++i) r.move_handles.push_back(Word(tls, at++));
    }
    for (unsigned i = 0; i < nx; ++i) {  // X: indice 0-5, endereco alto 6-11, medio 12-15, tamanho 16-31; endereco baixo
        const uint32_t a = Word(tls, at), b = Word(tls, at + 1);
        const uint64_t address = uint64_t{b} | (uint64_t{(a >> 12) & 0xF} << 32) | (uint64_t{(a >> 6) & 0x3F} << 36);
        r.x.push_back({address, a >> 16, a & 0x3F});
        at += 2;
    }
    const auto abw = [&](std::vector<Buffer>& out, unsigned count) {
        for (unsigned i = 0; i < count; ++i) {
            const uint32_t size_lo = Word(tls, at), addr_lo = Word(tls, at + 1), packed = Word(tls, at + 2);
            // modo 0-1, endereco alto 2-23, tamanho alto 24-27, endereco medio 28-31
            const uint64_t address = uint64_t{addr_lo} | (uint64_t{(packed >> 28) & 0xF} << 32) | (uint64_t{(packed >> 2) & 0x3FFFFF} << 36);
            const uint64_t size = uint64_t{size_lo} | (uint64_t{(packed >> 24) & 0xF} << 32);
            out.push_back({address, size, packed & 3});
            at += 3;
        }
    };
    abw(r.a, na);
    abw(r.b, nb);
    abw(r.w, nw);
    const size_t data_start = at;
    if (c_flags >= 2) {  // C: depois dos dados
        const size_t c_at = data_start + data_words;
        const unsigned count = c_flags == 2 ? 1 : c_flags - 2;
        for (unsigned i = 0; i < count; ++i) {
            const uint32_t lo = Word(tls, c_at + i * 2), hi = Word(tls, c_at + i * 2 + 1);
            r.c.push_back({BufferAddress(lo, hi & 0xFFFF), hi >> 16, 0});
        }
    }
    if (r.type >= 16) {  // TIPC
        r.tipc = true;
        r.command = r.type - 16;
        r.data.assign(tls + data_start * 4, tls + (data_start + data_words) * 4);
        return true;
    }
    if (r.type == 2) return true;  // Close
    // CMIF: dados alinhados a 16 bytes a partir do inicio do buffer
    size_t raw = (data_start * 4 + 15) & ~size_t{15};
    const size_t raw_end = (data_start + data_words) * 4;
    domain_object = 0;
    domain_command = 1;
    if (domain && (r.type == 4 || r.type == 6)) {
        const uint8_t dtype = tls[raw];
        const uint8_t in_count = tls[raw + 1];
        uint16_t dsize;
        std::memcpy(&dsize, tls + raw + 2, 2);
        std::memcpy(&domain_object, tls + raw + 4, 4);
        domain_command = dtype;
        const size_t payload = raw + 16;
        for (unsigned i = 0; i < in_count; ++i) r.in_objects.push_back(Word(tls + payload + dsize, i));
        raw = payload;
        if (dtype == 2) return true;  // fechar objeto do dominio
        if (Word(tls + raw, 0) != kSfci) {
            error = "dominio sem SFCI";
            return false;
        }
        r.command = Word(tls + raw, 2);
        r.data.assign(tls + raw + 16, tls + raw + dsize);
        return true;
    }
    if (Word(tls + raw, 0) != kSfci) {
        error = "requisicao CMIF sem SFCI";
        return false;
    }
    r.command = Word(tls + raw, 2);
    if (raw + 16 <= raw_end) r.data.assign(tls + raw + 16, tls + raw_end);
    return true;
}

// Escreve a resposta (CMIF ou TIPC) no TLS
void Write(uint8_t* tls, const Request& r, Response& s, bool domain, std::vector<uint32_t>& out_object_ids) {
    std::memset(tls, 0, 0x100);
    std::vector<uint32_t> moves = s.move_handles;
    if (!domain) {
        for (auto& object : s.objects) moves.push_back(CreateSessionHandle(object));
    }
    const bool handles = !s.copy_handles.empty() || !moves.empty();
    size_t at = 2;
    if (handles) {
        PutWord(tls, at++, static_cast<uint32_t>((s.copy_handles.size() << 1) | (moves.size() << 5)));
        for (uint32_t h : s.copy_handles) PutWord(tls, at++, h);
        for (uint32_t h : moves) PutWord(tls, at++, h);
    }
    size_t words = 0;
    if (r.tipc) {
        PutWord(tls, at, s.result);
        std::memcpy(tls + (at + 1) * 4, s.data.data(), s.data.size());
        words = 1 + (s.data.size() + 3) / 4;
    } else {
        const size_t raw = (at * 4 + 15) & ~size_t{15};
        size_t p = raw;
        if (domain) {  // CmifDomainOutHeader: numero de objetos de saida
            const uint32_t count = static_cast<uint32_t>(out_object_ids.size());
            std::memcpy(tls + p, &count, 4);
            p += 16;
        }
        const uint32_t header[4] = {kSfco, 0, s.result, 0};
        std::memcpy(tls + p, header, 16);
        std::memcpy(tls + p + 16, s.data.data(), s.data.size());
        p += 16 + ((s.data.size() + 3) & ~size_t{3});
        for (uint32_t id : out_object_ids) {
            std::memcpy(tls + p, &id, 4);
            p += 4;
        }
        words = (p - at * 4 + 3) / 4 + 4;  // inclui a folga de alinhamento
    }
    PutWord(tls, 0, 0);
    PutWord(tls, 1, static_cast<uint32_t>(words) | (handles ? 0x80000000u : 0));
}

}  // namespace

uint32_t CreateSessionHandle(std::shared_ptr<Service> service) {
    auto session = std::make_shared<Session>();
    session->service = std::move(service);
    return kernel::Insert(std::move(session));
}

bool CloseHandle(uint32_t handle) { return kernel::Close(handle); }

void ConnectToNamedPort(Context& c) {
    const std::string name(reinterpret_cast<const char*>(c.x[1]));
    auto service = MakeService(name);
    if (!service) Fatal(c, "[ipc] ConnectToNamedPort(\"%s\"): porta desconhecida", name.c_str());
    c.x[1] = CreateSessionHandle(service);
    Log("[ipc] ConnectToNamedPort(\"%s\") = 0x%x", name.c_str(), static_cast<uint32_t>(c.x[1]));
    c.x[0] = 0;
}

void SendSyncRequest(Context& c) {
    const uint32_t handle = static_cast<uint32_t>(c.x[0]);
    const std::shared_ptr<Session> session = kernel::GetAs<Session>(handle);
    if (!session) {
        c.x[0] = kResultInvalidHandle;
        return;
    }
    std::lock_guard session_lock(session->mutex);
    auto* tls = reinterpret_cast<uint8_t*>(c.tpidrro);
    Request request;
    uint32_t object_id = 0, domain_command = 1;
    std::string error;
    if (!Parse(tls, session->domain, request, object_id, domain_command, error)) {
        Fatal(c, "[ipc] %s: %s (tipo %u)", session->service->Name().c_str(), error.c_str(), request.type);
    }
    Response response;
    std::vector<uint32_t> out_ids;
    if (request.type == 2) {  // Close
        CloseHandle(handle);
        std::memset(tls, 0, 16);
        c.x[0] = 0;
        return;
    }
    std::shared_ptr<Service> target = session->service;
    if (session->domain && object_id != 0) {
        const auto it = session->objects.find(object_id);
        if (it == session->objects.end()) Fatal(c, "[ipc] dominio de %s: objeto %u inexistente", session->service->Name().c_str(), object_id);
        target = it->second;
        if (domain_command == 2) {
            session->objects.erase(it);
            Write(tls, request, response, true, out_ids);
            c.x[0] = 0;
            return;
        }
    }
    if (request.type == 5 || request.type == 7) {  // controle
        switch (request.command) {
        case 0:  // ConvertCurrentObjectToDomain
            session->domain = true;
            session->objects[session->next_object] = session->service;
            response.Push<uint32_t>(session->next_object++);
            break;
        case 3:  // QueryPointerBufferSize
            response.Push<uint16_t>(target->PointerBufferSize());
            break;
        case 2:  // CloneCurrentObject
        case 4:  // CloneCurrentObjectEx
            response.move_handles.push_back(CreateSessionHandle(target));
            break;
        default:
            Fatal(c, "[ipc] comando de controle %u nao implementado (%s)", request.command, target->Name().c_str());
        }
        Write(tls, request, response, false, out_ids);
        c.x[0] = 0;
        return;
    }
    target->Handle(c, request, response);
    if (session->domain) {
        for (auto& object : response.objects) {
            session->objects[session->next_object] = object;
            out_ids.push_back(session->next_object++);
        }
        response.objects.clear();
    }
    Write(tls, request, response, session->domain, out_ids);
    c.x[0] = 0;
}

} // namespace recomp::ipc
