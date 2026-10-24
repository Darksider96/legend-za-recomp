# TAREFA PARA O CODEX — Ficha de ABI das APIs que serão sobrescritas na R5

> **Tarefa NOVA.** A superfície de API da R5 foi aceita (`r5_api_surface*`, totais conferidos). Não edite arquivos
> de tarefas anteriores (`kernel_census*`, `r5_api_surface*` e os demais em `analysis/recomp/`).

## Contexto
Pela recomendação aceita, na R5 boa parte das funções `nn::` importadas pelo main será **sobrescrita por nome** com
implementação nativa (fs, oe/applet, account, time, settings, err, nifm, aoc, pl, vi, audio). Uma sobrescrita
recebe o `Context` do jogo e precisa saber, para cada função, **em que registrador chega cada argumento e onde
devolver o resultado** (AAPCS64), e o **tamanho/layout** dos tipos passados por valor ou devolvidos.

Entradas: `analysis/recomp/generated/r5_imports.csv` (as 310 importações das áreas), `imports.csv` (nome mangled e
demangled). Sem disassembler obrigatório; use Capstone no `build\venv` só se quiser conferir uso de `x8` no main.

## Arquivos que você pode criar (e SOMENTE estes)
- `analysis/recomp/r5_abi.py`
- `analysis/recomp/r5_abi_report.md`
- Saídas CSV **somente** em `analysis/recomp/generated/`

## Parte 1 — Classificação AAPCS64 mecânica (todas as 310, menos `nn::os`)
A partir da assinatura demangled (e do mangled para desambiguar), para cada argumento: tipo, classe e local:
- inteiros, ponteiros, referências (`T&`/`T const&` = ponteiro), enums, `bool` → próximo `x0..x7`;
- `float`/`double` → próximo `v0..v7`;
- `this` implícito em métodos (`Classe::metodo` não estático) → `x0` primeiro;
- struct/classe **por valor**: ≤ 16 bytes e sem membros não triviais → 1 ou 2 registradores `x`; > 16 bytes →
  ponteiro para cópia em `x`; HFA (até 4 floats/doubles iguais) → `v`;
- retorno: `void`; inteiro/ponteiro/`bool`/enum → `x0`/`w0`; `nn::Result` (struct de 4 bytes) → `w0`;
  float → `v0`; struct ≤ 16 bytes → `x0`(+`x1`); struct > 16 bytes → **resultado indireto em `x8`**.
Tipos com tamanho desconhecido ficam com classe `precisa_tamanho` (não chute).
Saída `generated/r5_abi.csv`: `importacao_mangled,demangled,area,arg_indice,arg_tipo,classe,local` (uma linha por
argumento, mais uma linha `retorno`).

## Parte 2 — Tabela de tipos
Para os tipos por valor ou de retorno que aparecem na Parte 1 (ex.: `nn::fs::FileHandle`, `DirectoryHandle`,
`nn::account::Uid`, `UserHandle`, `nn::ApplicationId`, `nn::TimeSpan`, `nn::time::PosixTime`,
`nn::util::BitFlagSet<32,…>`, `nn::hid::*State`, `nn::oe::*`, `nn::settings::LanguageCode`, `nn::Result`), registrar
tamanho, alinhamento e campos **com fonte pública citada** (libnx, Atmosphère, documentação do switchbrew; revisão
fixada como na tarefa anterior). Onde houver dúvida, marcar `nao_confirmado`. Quando possível, **confirmar o tamanho
no próprio binário**: por exemplo, o tamanho que o main reserva na pilha ou copia (`ldp/stp`) ao montar o argumento
antes do `BL` para o stub — registre a evidência (RVA do call site) no CSV.
Saída `generated/r5_types.csv` (`tipo,tamanho,alinhamento,campos,fonte,confirmado_no_binario,evidencia_rva`).

## Relatório
- Totais: argumentos por classe, funções com retorno indireto (`x8`), funções com algum `precisa_tamanho`.
- Uma tabela compacta por área: `função → x0..x7 / v0.. / retorno` (uma linha por função).
- A tabela de tipos com fonte e status de confirmação.

## Autoverificações (o script deve conferir e imprimir)
1. Toda importação de `r5_imports.csv` (exceto `nn::os`) tem linhas no `r5_abi.csv`; contagem igual.
2. Nenhuma função usa mais que 8 registradores `x` ou 8 `v` sem marcar os excedentes como `pilha`.
3. Todo tipo `precisa_tamanho` da Parte 1 aparece na Parte 2 (resolvido ou `nao_confirmado`).

## Critério de aceite
```
build\venv\Scripts\python.exe -I analysis\recomp\r5_abi.py
python tools\check_repo_safety.py
```

## Regras
- Se o mesmo erro acontecer duas vezes seguidas, **pare e relate**. Não tente de novo em loop.
- Não faça commit nem push.
