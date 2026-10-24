# TAREFAS PARA O ANTIGRAVITY — Lote 3

O lote 2 foi aceito (`run_all_tests.bat` e `gen_import_table.py`). Faça a **Tarefa A** e depois a **Tarefa B**.
Entregue um relatório por tarefa com a saída dos comandos de aceite.

## Regras (valem para as duas)
- Edite **somente** os arquivos listados em cada tarefa. Nada em `CMakeLists.txt`, `.gitignore`, manifesto,
  `recomp/src`, `recomp/include`, `recomp/tests`, `za_port/` e `analysis/`. Se achar que outro arquivo precisa
  mudar, escreva isso no relatório em vez de mudar.
- Se o mesmo erro acontecer duas vezes seguidas, **pare e relate**. Não tente de novo em loop.
- Não faça commit nem push.
- Releia o arquivo antes de editar (os dois já existem e são seus do lote 2).

---

## Tarefa A — etapa opcional de teste diferencial no `tools/run_all_tests.bat`

O recompilador agora tem um teste diferencial que compara instruções traduzidas com o emulador Unicorn
(`recomp/tools/insn_difftest.py`). Ele é lento e precisa de Python com `unicorn`, então entra como etapa opcional.

1. `tools\run_all_tests.bat` sem argumentos continua fazendo exatamente o que faz hoje.
2. `tools\run_all_tests.bat --difftest` faz, **depois** dos dois projetos:
   - procura `build\venv\Scripts\python.exe`; se não existir, ou se `build\venv\Scripts\python.exe -I -c "import unicorn"`
     falhar, imprime `difftest: PULADO (venv sem unicorn)` e segue;
   - se existir, roda `build\venv\Scripts\python.exe -I recomp\tools\insn_difftest.py build\recomp exefs\main`
     (só se `recomp` deu OK; senão `difftest: PULADO (recomp falhou)`).
3. O resumo final ganha a linha `difftest: OK`, `difftest: FALHOU` ou `difftest: PULADO (...)` quando a opção é usada.
   Código de saída 0 só se nada FALHOU (PULADO não reprova).
4. Para preparar o venv (documente no topo do .bat, como comentário `rem`):
   `python -m venv build\venv` e `build\venv\Scripts\pip install unicorn capstone`.

**Aceite** (cole o resumo final dos dois):
```
tools\run_all_tests.bat
tools\run_all_tests.bat --difftest
```
O primeiro igual ao lote 2 (dois OK, código 0); o segundo com `difftest: OK` e código 0 (com o venv preparado).

---

## Tarefa B — ordem de verdade no self-test do `recomp/tools/gen_import_table.py`

Na revisão do lote 2: o passo `[SELF-TEST] Ordenacao por (ns, mangled)` só confere se cada nome **aparece** no
código gerado, não a **ordem**. Corrija para conferir que as cinco entradas de `expected_order` aparecem no
`import_table.cpp` gerado exatamente nessa sequência (por exemplo, comparando as posições com `source.index(...)`).
Acrescente um caso negativo: gerar com a ordem trocada de propósito (chamando a formatação sem ordenar, ou
invertendo a lista) e confirmar que a checagem de ordem **reprova** esse caso.

**Aceite:**
```
python recomp/tools/gen_import_table.py --self-test
python recomp/tools/gen_import_table.py analysis/recomp/generated/imports.csv --out build/hle_table
```
O primeiro passa e mostra o caso negativo reprovado como esperado; o segundo continua informando **1001** importações.
