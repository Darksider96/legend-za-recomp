# legend-za-recomp

Recompilação estática (AArch64 → C++ → x86-64) de *Pokémon Legends: Z-A* para Windows.

O objetivo é executar o **código original do jogo**, traduzido para código nativo, sobre um runtime que substitui
o SDK da Nintendo (`nn::*`) e a API gráfica NVN (sobre Vulkan). Não é uma reimplementação do jogo.

## Este repositório não contém o jogo

Nada aqui inclui código, dados, chaves ou assets do jogo, e o código gerado pelo recompilador também não é
versionado (ele contém o código do jogo traduzido). Cada usuário precisa fornecer o próprio dump legal:

- `exefs/` (`main`, `sdk`, `rtld`, `main.npdm`)
- `romfs/` extraído

Não abra issues nem PRs com binários, dumps, chaves ou arquivos do jogo.

## Estrutura

| Pasta | Conteúdo |
|---|---|
| `recomp/` | Recompilador (ferramentas offline) e runtime do recomp — **trabalho ativo** |
| `za_port/` | Ferramentas de assets (TRPFS, BNTX, modelos, colisão, Wwise), testes e o protótipo anterior (reimplementação, congelada) |
| `analysis/` | Scripts e notas de engenharia reversa (os dumps do Ghidra ficam apenas locais) |

Veja `ORCHESTRATION_MANIFEST.md` para o plano e a divisão de trabalho.
