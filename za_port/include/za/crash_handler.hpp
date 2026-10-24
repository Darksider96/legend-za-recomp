#pragma once
#include <string>

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>

namespace za {

class CrashHandler {
public:
    // Instala os interceptores globais de exceção não tratada do Windows e sinais POSIX
    static void Install();

    // Atualiza o último ponto de verificação / subsistema ativo para contexto do dump
    static void SetLastCheckpoint(const char* checkpoint);
    static const char* GetLastCheckpoint();

    // Gera o relatório textual completo de crash e minidump
    static void WriteCrashDump(EXCEPTION_POINTERS* ep);
};

} // namespace za
