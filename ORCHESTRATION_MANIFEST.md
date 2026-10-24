# MANIFESTO DE ORQUESTRAÇÃO E GOVERNANÇA DO PROJETO
## PORT NATIVO PC WINDOWS x86_64 — POKÉMON LEGENDS: Z-A (TRINITY ENGINE)

### 1. Papéis e Domínios de Responsabilidade (Divisão Estrita de Propriedade)

| Agente | Domínio de Responsabilidade | Arquivos de Propriedade Exclusiva | Regra de Bloqueio |
| :--- | :--- | :--- | :--- |
| **Antigravity** *(Orquestrador & Integrador Geral)* | Arquitetura do motor, runtime Win32/GDI/NVN, loop 60 FPS, câmera 3D, HID/XInput, mixer de áudio (`AudioPlayback`), CMakeLists, builds finais e verificação | `src/main.cpp`, `src/services/*`, `include/services/*`, `CMakeLists.txt`, `za_port.exe` | Nunca editar arquivos de parsers de dados ou relatórios binários em andamento pelos colegas. |
| **Claude Code** *(Engenharia de Dados & Parsers)* | Reversão de estruturas FlatBuffers / binárias, tabelas da Pokédex, aprendizado de golpes, decodificadores de áudio/gráficos e suítes de testes unitários | `include/data/*`, `src/data/*`, `tests/*`, `include/gfx/scene_assembly.*`, `src/gfx/scene_assembly.*` | Nunca editar `main.cpp` ou subsistemas gráficos de baixo nível (`services/nvn.cpp`) diretamente. |
| **Codex** *(Engenharia Reversa AArch64 / Ghidra)* | Desmontagem do binário Switch (`text.bin`, `rodata.bin`), extração de VAs de subsistemas, fórmulas matemáticas originais e constantes do binário | `analysis/*` | Nunca editar código-fonte de implementação em `src/` ou `include/`. |

---

### 2. Status Atual do Projeto (Fase 11 Concluída)
- **Compilação Nativa:** Windows x86_64 via MinGW GCC C++20 (`za_port.exe`).
- **Apresentação Gráfica:** 60 FPS estável, Double Buffering com VSync, Shading 3D sólido iluminado (Z-buffer) e aramado alternável.
- **Câmera:** Câmera de terceira pessoa suave seguindo o Pokémon ativo + modo Voo Livre 360 graus com controle por Mouse.
- **Cenário & Colisão:** Arena de Lumiose City (`t1_battle1_0.trscn`) + 3 setores urbanos (`t1_bk01..03`) com 51 malhas Havok e detecção contínua de solo `RaycastGround`.
- **Áudio:** Trilha sonora tema de Lumiose City Main Street (`226064614`) em streaming contínuo PTADPCM 48 kHz estéreo + Cries canônicos e SFX de impacto.
- **Pokédex Regional:** 1.422 espécies decodificadas com atributos canônicos, tipos, altura, peso e nomes decifrados em inglês.
- **Sistema de Golpes (Fase 11):** 902 golpes carregados de `waza_array.bin` + `wazaname.dat`, matriz elementar 18x18, seletor de golpes interativo via [Z][X][C][V] e D-Pad, e banner dinâmico de combate.
- **Suíte de Testes:** **145 de 145 testes passando com 100% de sucesso nas 6 suítes (0 falhas)!**

---

### 3. Fases Ativas
- **Claude Code:** Fase 13 (`CLAUDE_TASK.md`) — Configurador Inicial (`za_configurator`), Sistema de Persistência `za_config.ini`, detecção de `.nsp` e testes unitários (`tests/config_test.cpp`).
- **Antigravity:** Monitoramento, testes integrados de regressão, governança de código e manutenção do build.
