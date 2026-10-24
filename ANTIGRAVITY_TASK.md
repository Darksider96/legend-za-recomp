# TAREFA PARA O ANTIGRAVITY — Lote 4: testes unitários das bordas de ponto flutuante

O lote 3B foi aceito (self-test de ordem). O 3A (`--difftest`) será conferido pelo Claude Code quando a R3 compilar.

O recompilador ganhou a semântica SIMD/FP em `recomp/include/recomp/simd.hpp`. O teste diferencial contra o Unicorn
usa valores sorteados e quase nunca acerta casos de borda (dois zeros com sinais diferentes, NaN sinalizador, empates de
arredondamento). Esta tarefa cria testes com valores **fixos e conhecidos** para essas bordas.

## Arquivos
- **Criar** `recomp/tests/simd_test.cpp` (mesmo estilo de `recomp/tests/context_test.cpp`: função `Check`, contador de
  falhas, imprime `OK`/`FALHOU` no fim e devolve 0 só sem falhas).
- **Editar** `recomp/CMakeLists.txt` **somente** para acrescentar estas 4 linhas logo depois do bloco do `context_test`:
  ```cmake
  add_executable(simd_test tests/simd_test.cpp)
  target_include_directories(simd_test PRIVATE include)
  target_compile_options(simd_test PRIVATE -Wall -Wextra -ffp-contract=off)
  add_test(NAME simd_test COMMAND simd_test)
  ```
- Não edite `simd.hpp` nem nenhum outro arquivo. Se um caso abaixo falhar, **não mude o caso nem o header**: deixe o
  teste falhando e relate o caso exato (valor obtido em hexadecimal) — pode ser bug do header, e quem decide é o Claude Code.

Use `std::bit_cast` para comparar bits (NaN e sinal de zero não se comparam com `==`).

## Casos obrigatórios
1. **Meia precisão, ida e volta exaustiva**: para todo `h` de 0 a 0xFFFF:
   - se `h` não é NaN (expoente 0x1F com fração ≠ 0): `ToHalf(FromHalf<float>(h)) == h` e
     `ToHalf(FromHalf<double>(h)) == h`;
   - se é NaN: o resultado é `h | 0x0200` (NaN silenciado, mesmo payload).
   Conte e imprima quantos valores passaram (deve ser 65536).
2. **Arredondamento para meia** (`ToHalf(float)`): `65504.0f` → `0x7BFF`; `65520.0f` → `0x7C00`; `-65520.0f` → `0xFC00`;
   `0x1p-24f` → `0x0001`; `0x1p-25f` → `0x0000` (empate vai para o par); `0x1.8p-25f` → `0x0001`;
   `1.0f + 0x1p-11f` → `0x3C00` (empate, par); `1.0f + 0x1.8p-10f` → `0x3C02`; `-0.0f` → `0x8000`.
3. **Conversão FP → inteiro** (`FpToInt<I, Modo>`; modos: 0 mais próximo, 1 +inf, 2 −inf, 3 zero, 4 empate longe do zero):
   - `FpToInt<int32_t, 3>(3e9f)` = `INT32_MAX`; `FpToInt<int32_t, 3>(-3e9f)` = `INT32_MIN`;
   - `FpToInt<int32_t, 3>(NaN)` = 0; `FpToInt<uint32_t, 3>(-1.0f)` = 0; `FpToInt<uint64_t, 3>(1e30)` = `UINT64_MAX`;
   - `FpToInt<int32_t, 0>(2.5f)` = 2; `FpToInt<int32_t, 0>(3.5f)` = 4; `FpToInt<int32_t, 4>(2.5f)` = 3;
     `FpToInt<int32_t, 4>(-2.5f)` = −3; `FpToInt<int32_t, 2>(-0.5f)` = −1; `FpToInt<int32_t, 1>(0.2f)` = 1;
   - ponto fixo: `FpToInt<int32_t, 3, 8>(1.5f)` = 384.
4. **FMAX/FMIN e zeros/NaN** (float):
   - `FpMax(-0.0f, 0.0f)` e `FpMax(0.0f, -0.0f)` têm bits `0x00000000`; `FpMin` dos mesmos pares tem bits `0x80000000`;
   - `FpMax(qNaN, 1.0f)` é NaN; `FpMaxNum(qNaN, 1.0f)` = 1.0f; `FpMinNum(1.0f, qNaN)` = 1.0f;
   - `FpMax(sNaN 0x7FA00001, 1.0f)` tem bits `0x7FE00001` (silenciado, mesmo payload);
   - `FpMaxNum(qNaN, sNaN 0x7FA00001)` tem bits `0x7FE00001`.
5. **Propagação de NaN** (float): `FpAdd(qNaN 0x7FC00001, qNaN 0x7FC00002)` tem bits `0x7FC00001` (o primeiro);
   `FpAdd(qNaN 0x7FC00001, sNaN 0x7F800002)` tem bits `0x7FC00002` (o sinalizador tem prioridade, silenciado);
   `FpAdd(+inf, -inf)` tem bits `0x7FC00000` (NaN padrão do ARM, positivo); `FpMul(0.0f, inf)` idem.
6. **FMA fundida**: `FpMulAdd(-(1.0f + 0x1p-11f), 1.0f + 0x1p-12f, 1.0f + 0x1p-12f)` = `0x1p-24f` (o produto exato é
   1 + 2⁻¹¹ + 2⁻²⁴; arredondado antes da soma daria 0);
   `FpMulAdd(qNaN, 0.0f, inf)` tem bits `0x7FC00000` (NaN padrão: 0 × inf com addend NaN silencioso).
7. **Estimativas**: `FpRecipEstimate(1.0f)` tem bits `0x3F7F8000`; `FpRsqrtEstimate(1.0f)` tem bits `0x3F7F8000`;
   `FpRsqrtEstimate(4.0f)` tem bits `0x3EFF8000`; `FpRecipEstimate(0.0f)` = +inf; `FpRsqrtEstimate(-1.0f)` tem bits
   `0x7FC00000`.

## Aceite (cole a saída)
```
tools\run_all_tests.bat
build\recomp\simd_test.exe
```
O `simd_test` lista todos os casos. Se tudo passar, `recomp: OK` e `za_port: OK`. Se algum caso falhar, entregue assim
mesmo com o relatório dos casos que falharam (valor esperado e obtido em hexadecimal).

## Regras
- Se o mesmo erro de compilação acontecer duas vezes seguidas, **pare e relate**.
- Não faça commit nem push.
