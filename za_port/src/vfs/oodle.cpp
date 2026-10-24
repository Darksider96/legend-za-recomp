#include "vfs/trpfs.hpp"

#include <cstdlib>
#include <filesystem>
#include <iostream>
#include <mutex>

#ifdef _WIN32
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>
#endif

namespace za::vfs::oodle {

namespace {

constexpr const char* kDllName = "oo2core_9_win64.dll";

// intptr_t OodleLZ_Decompress(compBuf, compSize, rawBuf, rawSize, fuzzSafe, checkCRC, verbosity,
//                             decBufBase, decBufSize, fpCallback, callbackUserData,
//                             decoderMemory, decoderMemorySize, threadPhase)
using DecompressFn = intptr_t (*)(const void*, intptr_t, void*, intptr_t, int, int, int,
                                  void*, intptr_t, void*, void*, void*, intptr_t, int);

DecompressFn g_decompress = nullptr;
std::once_flag g_load_once;

#ifdef _WIN32
HMODULE TryLoad(const std::filesystem::path& path) {
    if (path.empty()) return nullptr;
    return LoadLibraryW(path.c_str());
}

std::filesystem::path ExecutableDir() {
    wchar_t buffer[MAX_PATH] = {};
    DWORD len = GetModuleFileNameW(nullptr, buffer, MAX_PATH);
    if (len == 0 || len == MAX_PATH) return {};
    return std::filesystem::path(buffer).parent_path();
}

void LoadImpl() {
    HMODULE module = nullptr;
    std::string source;

    // 1) Mesmo diretorio do za_port.exe
    if (auto dir = ExecutableDir(); !dir.empty()) {
        module = TryLoad(dir / kDllName);
        if (module) source = (dir / kDllName).string();
    }

    // 2) ZA_OODLE_DLL: caminho completo da DLL ou diretorio que a contem
    if (!module) {
        if (const char* env = std::getenv("ZA_OODLE_DLL"); env && *env) {
            std::filesystem::path p(env);
            std::error_code ec;
            if (std::filesystem::is_directory(p, ec)) p /= kDllName;
            module = TryLoad(p);
            if (module) source = p.string();
        }
    }

    // 3) Busca padrao do Windows (PATH)
    if (!module) {
        module = LoadLibraryA(kDllName);
        if (module) source = std::string(kDllName) + " (PATH)";
    }

    if (!module) {
        std::cout << "[TRPFS] Aviso: oo2core_9_win64.dll nao encontrada. Descompressao Oodle desativada "
                     "(apenas arquivos uncompressed serao servidos)." << std::endl;
        return;
    }

    FARPROC proc = GetProcAddress(module, "OodleLZ_Decompress");
    if (!proc) {
        std::cout << "[TRPFS] Aviso: OodleLZ_Decompress ausente em " << source
                  << ". Descompressao Oodle desativada." << std::endl;
        FreeLibrary(module);
        return;
    }

    // void(*)() e o tipo coringa do GCC: evita -Wcast-function-type
    g_decompress = reinterpret_cast<DecompressFn>(reinterpret_cast<void (*)()>(proc));
    std::cout << "[TRPFS] Oodle carregado: " << source << std::endl;
}
#else
void LoadImpl() {
    std::cout << "[TRPFS] Aviso: oo2core_9_win64.dll nao encontrada. Descompressao Oodle desativada "
                 "(apenas arquivos uncompressed serao servidos)." << std::endl;
}
#endif

} // namespace

bool Load() {
    std::call_once(g_load_once, LoadImpl);
    return g_decompress != nullptr;
}

bool IsAvailable() {
    return Load();
}

bool Decompress(const uint8_t* src, size_t src_size, uint8_t* dst, size_t dst_size) {
    if (!Load() || !src || !dst) return false;

    // fuzzSafe=1, checkCRC=0, verbosity=0, threadPhase=3 (decodificacao completa em uma thread)
    intptr_t written = g_decompress(src, static_cast<intptr_t>(src_size),
                                    dst, static_cast<intptr_t>(dst_size),
                                    1, 0, 0, nullptr, 0, nullptr, nullptr, nullptr, 0, 3);
    return written == static_cast<intptr_t>(dst_size);
}

} // namespace za::vfs::oodle
