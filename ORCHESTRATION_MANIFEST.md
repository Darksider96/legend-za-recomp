# MANIFESTO DO PROJETO — legend-za-recomp
## Recompilação estática de Pokémon Legends: Z-A (Switch, AArch64) para Windows x86-64

### 1. Direção
O projeto deixou de ser uma reimplementação. O objetivo é **executar o código original do `exefs/main`**, traduzido
para C++ e compilado para x86-64, sobre um runtime que substitui apenas o que está fora do `main`:
o SDK da Nintendo (`nn::*`) e a API gráfica NVN. A lógica do jogo, Trinity, Havok, Wwise e Oodle já estão dentro do
`main` (não há `subsdk`) e serão recompilados — não reescritos.

O protótipo anterior (`za_port.exe`: renderer próprio, combate, câmera, atores) está **congelado**: nada novo nele.
As ferramentas de assets de `za_port/` continuam valendo como ferramentas de inspeção e como oráculos de teste.

### 2. Liderança e papéis
| Agente | Papel | Pode editar |
| :--- | :--- | :--- |
| **Claude Code** | Liderança técnica: arquitetura do recomp, recompilador, runtime, testes, revisão de tudo que entra em `main` | todo o repositório |
| Outros agentes | Tarefas delegadas por escrito em `CLAUDE_TASK.md`/`CODEX_TASK.md`, com lista exata de arquivos | apenas os arquivos listados na tarefa |

Regras para qualquer agente:
1. **Antes de criar um arquivo, verificar se ele já existe; antes de editar, reler.** Nunca sobrescrever trabalho alheio.
2. Uma tarefa por vez, com critério de aceite verificável (teste ou comando). Sem critério, não começa.
3. Se o mesmo erro aparecer duas vezes seguidas, **parar e relatar** em vez de tentar de novo (evita loops).
4. Commits pequenos, mensagem explicando o porquê; nada vai para `main` sem build e testes verdes.
5. **Nunca versionar** dump, chaves, assets, código descompilado/desmontado ou código gerado pelo recompilador
   (ver `.gitignore`). O repositório é público em potencial.

### 3. Arquitetura alvo
| Camada | Responsabilidade |
| :--- | :--- |
| `recomp/tools` (offline) | Ler o NSO, descobrir funções (`.eh_frame`, símbolos, alvos de `BL`, vtables, tabelas de salto), traduzir cada função AArch64 para C++ |
| `recomp/runtime` | Memória do jogo no mesmo endereço do Switch (`0x7100000000`), segmentos e relocações, contexto de CPU (X0–X30, SP, NZCV, V0–V31, TLS), threads, tabela de despacho para chamadas indiretas |
| `recomp/hle` | Reimplementação nativa das funções `nn::` importadas (os, fs, hid, audio, vi, time, oe, account, rede como offline) |
| `recomp/nvn` | NVN → Vulkan, incluindo tradução dos shaders Maxwell (`.bnsh`/`.trsha`) para SPIR-V |
| testes | Testes diferenciais: função traduzida × referência validada (parsers de `za_port`, valores conhecidos) |

Fronteira conhecida: ~600 símbolos `nn::` e 533 funções NVN (via `nvnBootstrapLoader`); 50 MB de `.text`.

### 4. Fases
| Fase | Entrega | Critério de aceite |
| :--- | :--- | :--- |
| **R0** | Repositório, `.gitignore` seguro, manifesto | `git ls-files` sem material protegido |
| **R1** | Inventário do binário (`nso_inspect`): segmentos, importações, exportações, relocações, funções do `.eh_frame` | `.text` descomprimido idêntico ao export do Ghidra; `nnMain`/`nninitStartup` com tamanhos do relatório de boot |
| **R2** | Prova de conceito do tradutor: função de hash FNV-1a 64 do TRPFS traduzida | Saída idêntica a `za::vfs::Fnv1a64` em entradas aleatórias |
| R3 | Tradutor para funções puras (inteiro + NEON básico) com testes diferenciais | Lote de funções traduzidas = referência |
| R4 | Runtime mínimo: `nninitStartup` → `nnMain` registrando cada chamada `nn::` | Log de boot até a primeira importação não implementada |
| R5 | HLE de `nn::os/fs/hid/time/oe` até o loop principal | Jogo chega ao loop de frame sem travar |
| R6 | NVN → Vulkan e shaders | Primeira imagem do jogo na tela |
| R7 | Áudio (`nn::audio` → WASAPI), entrada, estabilidade | Jogo jogável |

### 5. Estado
- R0 concluída (2026-10-24).
- R1 concluída (2026-10-24): `nso_inspect` validou segmentos SHA-256, 1.106 símbolos, 346.629 relocações DT_RELR, 1.912 construtores estáticos e 222.376 funções no `.eh_frame` (98.9% do `.text`). Arquivos `imports.csv`, `exports.csv` e `functions.csv` gerados em `analysis/recomp/generated`.
- R2 em andamento.
