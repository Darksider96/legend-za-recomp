#include "za/crash_handler.hpp"
#include "za/logger.hpp"
#include <cstdio>
#include <csignal>
#include <cstring>
#include <chrono>
#include <ctime>
#include <atomic>

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>
#include <psapi.h>

namespace za {

static const char* g_last_checkpoint = "Startup / Inicializacao";
static std::atomic<bool> g_handling_crash{false};

void CrashHandler::SetLastCheckpoint(const char* checkpoint) {
    if (checkpoint) g_last_checkpoint = checkpoint;
}

const char* CrashHandler::GetLastCheckpoint() {
    return g_last_checkpoint;
}

typedef BOOL(WINAPI* PFN_MINIDUMP_WRITE_DUMP)(
    HANDLE hProcess,
    DWORD ProcessId,
    HANDLE hFile,
    DWORD DumpType,
    PVOID ExceptionParam,
    PVOID UserStreamParam,
    PVOID CallbackParam
);

static const char* GetExceptionDescription(DWORD code) {
    switch (code) {
        case EXCEPTION_ACCESS_VIOLATION:         return "EXCEPTION_ACCESS_VIOLATION (Violacao de Acesso a Memoria)";
        case EXCEPTION_ARRAY_BOUNDS_EXCEEDED:    return "EXCEPTION_ARRAY_BOUNDS_EXCEEDED (Indice Fora dos Limites)";
        case EXCEPTION_BREAKPOINT:               return "EXCEPTION_BREAKPOINT (Ponto de Interrupcao)";
        case EXCEPTION_DATATYPE_MISALIGNMENT:    return "EXCEPTION_DATATYPE_MISALIGNMENT (Desalinhamento de Dados)";
        case EXCEPTION_FLT_DIVIDE_BY_ZERO:       return "EXCEPTION_FLT_DIVIDE_BY_ZERO (Divisao por Zero Float)";
        case EXCEPTION_FLT_OVERFLOW:             return "EXCEPTION_FLT_OVERFLOW (Overflow de Ponto Flutuante)";
        case EXCEPTION_ILLEGAL_INSTRUCTION:      return "EXCEPTION_ILLEGAL_INSTRUCTION (Instrucao CPU Ilegal/Invalida)";
        case EXCEPTION_INT_DIVIDE_BY_ZERO:       return "EXCEPTION_INT_DIVIDE_BY_ZERO (Divisao Inteira por Zero)";
        case EXCEPTION_INT_OVERFLOW:             return "EXCEPTION_INT_OVERFLOW (Overflow de Inteiro)";
        case EXCEPTION_INVALID_DISPOSITION:      return "EXCEPTION_INVALID_DISPOSITION (Disposicao Invalida)";
        case EXCEPTION_NONCONTINUABLE_EXCEPTION: return "EXCEPTION_NONCONTINUABLE_EXCEPTION (Excecao Nao Continuavel)";
        case EXCEPTION_PRIV_INSTRUCTION:         return "EXCEPTION_PRIV_INSTRUCTION (Instrucao Privilegiada)";
        case EXCEPTION_STACK_OVERFLOW:           return "EXCEPTION_STACK_OVERFLOW (Estouro de Pilha)";
        default:                                 return "EXCECAO DESCONHECIDA";
    }
}

void CrashHandler::WriteCrashDump(EXCEPTION_POINTERS* ep) {
    if (g_handling_crash.exchange(true)) {
        return; // Evita recursão caso ocorra crash dentro do handler
    }

    FILE* f = std::fopen("za_crash_report.log", "w");
    if (!f) f = stderr;

    auto now = std::chrono::system_clock::now();
    std::time_t now_c = std::chrono::system_clock::to_time_t(now);
    std::tm tm_buf{};
    localtime_s(&tm_buf, &now_c);

    char time_str[64];
    std::strftime(time_str, sizeof(time_str), "%Y-%m-%d %H:%M:%S", &tm_buf);

    std::fprintf(f, "===============================================================================\n");
    std::fprintf(f, "        RELATORIO DE CRASH FATAL — POKEMON LEGENDS: Z-A (PC PORT x86_64)        \n");
    std::fprintf(f, "===============================================================================\n\n");

    std::fprintf(f, "Data e Hora do Incidente : %s\n", time_str);
    std::fprintf(f, "Process ID               : %lu (0x%lX)\n", GetCurrentProcessId(), GetCurrentProcessId());
    std::fprintf(f, "Thread ID                : %lu (0x%lX)\n", GetCurrentThreadId(), GetCurrentThreadId());
    std::fprintf(f, "Ultimo Ponto de Execucao : %s\n\n", g_last_checkpoint);

    // 1. Dados da Exceção
    if (ep && ep->ExceptionRecord) {
        DWORD code = ep->ExceptionRecord->ExceptionCode;
        void* addr = ep->ExceptionRecord->ExceptionAddress;
        std::fprintf(f, "[DADOS DA EXCECAO]\n");
        std::fprintf(f, "  Codigo        : 0x%08lX -> %s\n", code, GetExceptionDescription(code));
        std::fprintf(f, "  Endereco Falha: 0x%p\n", addr);

        if (code == EXCEPTION_ACCESS_VIOLATION && ep->ExceptionRecord->NumberParameters >= 2) {
            ULONG_PTR op = ep->ExceptionRecord->ExceptionInformation[0];
            ULONG_PTR target_addr = ep->ExceptionRecord->ExceptionInformation[1];
            const char* op_str = (op == 0) ? "LEITURA" : (op == 1 ? "ESCRITA" : "EXECUCAO");
            std::fprintf(f, "  Detalhes AV   : Tentativa de %s no endereco 0x%016llX\n", op_str, (unsigned long long)target_addr);
        }
        std::fprintf(f, "\n");
    }

    // 2. Registradores da CPU (x86_64)
    if (ep && ep->ContextRecord) {
        const CONTEXT* ctx = ep->ContextRecord;
        std::fprintf(f, "[REGISTRADORES DA CPU x86_64]\n");
        std::fprintf(f, "  RIP: 0x%016llX  RSP: 0x%016llX  RBP: 0x%016llX\n", ctx->Rip, ctx->Rsp, ctx->Rbp);
        std::fprintf(f, "  RAX: 0x%016llX  RBX: 0x%016llX  RCX: 0x%016llX\n", ctx->Rax, ctx->Rbx, ctx->Rcx);
        std::fprintf(f, "  RDX: 0x%016llX  RSI: 0x%016llX  RDI: 0x%016llX\n", ctx->Rdx, ctx->Rsi, ctx->Rdi);
        std::fprintf(f, "  R8 : 0x%016llX  R9 : 0x%016llX  R10: 0x%016llX\n", ctx->R8,  ctx->R9,  ctx->R10);
        std::fprintf(f, "  R11: 0x%016llX  R12: 0x%016llX  R13: 0x%016llX\n", ctx->R11, ctx->R12, ctx->R13);
        std::fprintf(f, "  R14: 0x%016llX  R15: 0x%016llX  EFL: 0x%08lX\n\n", ctx->R14, ctx->R15, ctx->EFlags);
    }

    // 3. Informações de Memória do Sistema
    MEMORYSTATUSEX mem_stat{};
    mem_stat.dwLength = sizeof(mem_stat);
    if (GlobalMemoryStatusEx(&mem_stat)) {
        std::fprintf(f, "[ESTADO DE MEMORIA DO SISTEMA]\n");
        std::fprintf(f, "  RAM Total Fisica : %llu MB\n", mem_stat.ullTotalPhys / (1024 * 1024));
        std::fprintf(f, "  RAM Livre Fisica : %llu MB\n", mem_stat.ullAvailPhys / (1024 * 1024));
        std::fprintf(f, "  Carga de Memoria : %lu%%\n\n", mem_stat.dwMemoryLoad);
    }

    // 4. Captura do Call Stack (Backtrace)
    void* stack[32];
    USHORT frames = RtlCaptureStackBackTrace(0, 32, stack, nullptr);
    std::fprintf(f, "[PILHA DE CHAMADAS (CALL STACK - %u FRAMES)]\n", frames);
    for (USHORT i = 0; i < frames; ++i) {
        HMODULE hMod = nullptr;
        char mod_name[MAX_PATH] = "Desconhecido";
        if (GetModuleHandleExA(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS | GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT,
                               (LPCSTR)stack[i], &hMod) && hMod) {
            GetModuleFileNameA(hMod, mod_name, sizeof(mod_name));
            char* slash = std::strrchr(mod_name, '\\');
            if (slash) std::memmove(mod_name, slash + 1, std::strlen(slash + 1) + 1);
        }
        std::fprintf(f, "  #%02u: 0x%p  [%s]\n", i, stack[i], mod_name);
    }
    std::fprintf(f, "\n");

    // 5. Geração de MiniDump (.dmp) para análise via Visual Studio / WinDbg
    HMODULE hDbgHelp = LoadLibraryA("dbghelp.dll");
    if (hDbgHelp) {
        auto pfnDump = (PFN_MINIDUMP_WRITE_DUMP)GetProcAddress(hDbgHelp, "MiniDumpWriteDump");
        if (pfnDump) {
            HANDLE hDumpFile = CreateFileA("za_crash.dmp", GENERIC_WRITE, 0, nullptr, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, nullptr);
            if (hDumpFile != INVALID_HANDLE_VALUE) {
                struct {
                    DWORD ThreadId;
                    PEXCEPTION_POINTERS ExceptionPointers;
                    BOOL ClientPointers;
                } exp_info{ GetCurrentThreadId(), ep, FALSE };

                BOOL success = pfnDump(GetCurrentProcess(), GetCurrentProcessId(), hDumpFile, 2 /* MiniDumpWithDataSegs */, &exp_info, nullptr, nullptr);
                CloseHandle(hDumpFile);
                if (success) {
                    std::fprintf(f, "[MINIDUMP] Arquivo binario de dump gravado com sucesso: za_crash.dmp\n");
                }
            }
        }
        FreeLibrary(hDbgHelp);
    }

    std::fprintf(f, "\n============================ FIM DO RELATORIO ============================\n");
    if (f != stderr) {
        std::fclose(f);
    }

    // Alerta visual no Console
    std::fprintf(stderr, "\n\n!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!\n");
    std::fprintf(stderr, "  [CRASH DETECTADO] O jogo encontrou uma falha fatal!\n");
    std::fprintf(stderr, "  Relatorio completo salvo em: za_crash_report.log\n");
    std::fprintf(stderr, "  Arquivo de dump salvo em    : za_crash.dmp\n");
    std::fprintf(stderr, "!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!\n\n");
    std::fflush(stderr);
}

static LONG WINAPI UnhandledExceptionHandler(EXCEPTION_POINTERS* ep) {
    CrashHandler::WriteCrashDump(ep);
    return EXCEPTION_EXECUTE_HANDLER;
}

static void PosixSignalHandler(int sig) {
    std::fprintf(stderr, "\n[SINAL FATAL INTERCEPTADO]: %d\n", sig);
    CrashHandler::WriteCrashDump(nullptr);
    std::abort();
}

void CrashHandler::Install() {
    SetUnhandledExceptionFilter(UnhandledExceptionHandler);
    std::signal(SIGSEGV, PosixSignalHandler);
    std::signal(SIGABRT, PosixSignalHandler);
    std::signal(SIGFPE,  PosixSignalHandler);
    std::signal(SIGILL,  PosixSignalHandler);

    ZA_LOG_INFO("CRASH-HANDLER", "Interceptor de falhas do Windows (SetUnhandledExceptionFilter) ativo.");
}

} // namespace za
