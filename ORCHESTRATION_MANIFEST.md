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
| `recomp/hle` | HLE do kernel (SVCs) e dos serviços do sistema; sobrescritas nativas de funções do sdk (NVN, fs, hid...) |
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
- R1 concluída (2026-10-24) — `recomp/tools/nso_inspect`, testes em `recomp/tests/nso_test.cpp`:
  - `.text` descomprimido idêntico ao export do Ghidra; SHA-256 dos 3 segmentos confere com o cabeçalho NSO.
  - **222.376 funções** no `.eh_frame` (98,9% do `.text`, sem sobreposição).
  - **1.001 importações** (946 funções), 987 resolvidas no `sdk`; 14 sem definição (`glslc*`, hooks ZSTD do pia,
    TLS do gRPC — prováveis símbolos fracos). Maiores grupos: libc 266, `nn::os` 102, `std` 92, `nn::hid` 56.
  - NVN entra por **uma** importação (`nvnBootstrapLoader`); as 533 funções são resolvidas por nome em runtime.
  - Relocações: 346.629 relativas (DT_RELR), 3.303 ABS64, 917 JUMP_SLOT, 55 GLOB_DAT; 1.912 construtores no INIT_ARRAY.
  - Saída em `analysis/recomp/generated/` (não versionada).
- R2 concluída (2026-10-24) — decodificador A64 (`recomp/src/a64_decode.cpp`), gerador de C++ (`emit_c.cpp`),
  `a64_recomp`, `a64_dump`, `validate_decoder.py`:
  - Decodificador × Capstone em 870.393 instruções amostradas: **100% de concordância**, 0 alvos e 0 registradores
    divergentes. Cobre 91,4% das 12,38 M instruções; o resto é SIMD/FP (~5,5%) e exclusivos/acquire-release (~3,1%).
  - `f_133f0c0` (FNV-1a 64 do jogo) traduzida no build a partir do dump: **100.000/100.000** strings aleatórias com
    hash idêntico a `za::vfs::Fnv1a64` (`fnv_poc_test`). Helpers semânticos testados em `context_test`.
  - **141.434 funções (63,6%)** já traduzem sem nenhuma instrução pendente; as demais param em SIMD/FP ou atômicos.
- R3 concluída (2026-10-24) — SIMD/FP (`a64_simd.cpp`, `emit_simd.cpp`, `recomp/simd.hpp`), exclusivos e acquire/release:
  - **222.238 de 222.376 funções (99,94%)** traduzem sem pendência; restam 221 instruções não decodificadas em 12,38 M.
  - Teste diferencial por instrução contra o Unicorn (`a64_insn_tests` + `insn_difftest` + `insn_difftest.py`), com
    valores de borda de FP nos vetores: **8.118 instruções reais / 1.249 formas / 129.888 casos: 0 divergências**.
    Pegou o estouro intermediário do FRSQRTS e o FPCR com bits não implementados.
  - `simd_test` (Antigravity): bordas fixas de FP — meia precisão exaustiva (65.536), arredondamentos, saturação,
    zeros/NaN de FMAX/FMIN, propagação de NaN, FMA fundida, FRECPE/FRSQRTE.
  - Semântica de FP do ARM com FPCR = 0 (arredondamento ao mais próximo, sem flush-to-zero); FPSR não é emulado.
    Código gerado com `-ffp-contract=off` (sem fusão de multiplicação e soma entre instruções).
- R4 em andamento (Claude Code): `recomp/runtime/` (carregador, despacho, kernel, `za_recomp`), build com `-DRECOMP_GAME=ON`.
  - `a64_recomp --all` traduz o módulo inteiro em arquivos fatiados: main = 224.328 funções (.eh_frame + 918 stubs
    da PLT + 1.034 das lacunas), ~1,1 GB de C++; ~54 s por arquivo de 15 MB em `-O1`.
  - Boot como no Horizon: o `rtld` chama `nn::init::Start` do sdk com 3 callbacks (pronto, init dos módulos via
    `DT_INIT` na ordem reversa de carga, fini); o sdk chama `nninitStartup` e `nnMain` do main.
- Levantamentos delegados aceitos (2026-10-24):
  - Codex, `analysis/recomp/control_flow_report.md`: 917 stubs da PLT mapeados (o 918º padrão é o PLT0);
    682.939 BL e 63.232 tail calls; 17 alvos em lacunas do `.eh_frame` (faixa 0x1d92734–0x1dcacd0, sem FDE: a R4
    precisa descobrir essas funções); 4.454 tabelas de salto candidatas, 3.990 com base e tamanho resolvidos.
  - Antigravity: `tools/run_all_tests.bat` (recomp e za_port verdes; `--difftest` opcional) e
    `recomp/tools/gen_import_table.py` (tabela HLE com as 1.001 importações).
  - Codex, `analysis/recomp/gap_functions_report.md`: 1.034 funções propostas para a lacuna de 230 KB; 0 alvos órfãos.

### 6. Decisões de arquitetura
- **O `sdk` também é recompilado** (2026-10-24). Ele traz libc (musl), libc++ e `nn::*` com o layout de objetos que o
  jogo espera (96 importações do main são do libc++); reimplementar isso como HLE seria frágil. A fronteira nativa
  fica nas **SVCs** (kernel do Horizon) e em sobrescritas pontuais por nome de função (NVN, fs, hid, áudio...) onde
  for mais simples que emular o serviço. O `rtld` não é traduzido: o carregador do runtime faz o papel dele.
- Endereços: main em `0x7100000000`, sdk em `0x7108000000`; funções nativas em endereços falsos `0x7F00000000 + 16·i`
  (o GOT recebe esses valores e o `Dispatch` os reconhece).
- Memória do jogo mapeada no host no **mesmo endereço** do Switch: ponteiros do jogo = ponteiros do host. Isso
  permite que importações de libc (`memcpy`, `strlen`...) chamem a implementação do host diretamente.
- Relocações (RELA, RELR, JMPREL) e construtores do INIT_ARRAY aplicados pelo runtime, como o `rtld` faria.
- O decodificador AArch64 do recompilador é próprio (sem dependência no runtime). Disassemblers externos (Capstone,
  Ghidra) entram só como oráculo de teste do decodificador.
- Código gerado nunca é versionado: o build gera a partir do dump do usuário.
- Executáveis MinGW sempre com `-static` (um `libstdc++` de outra instalação no PATH derruba o processo).
- Código gerado compilado com `-mcx16` (cmpxchg16b para LDXP/STXP) e `-ffp-contract=off`.
