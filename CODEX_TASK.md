# ESCOPO DE TRABALHO PARA CODEX (~1 HORA)
## FASE 8: ESTRUTURA DE ESTÁGIOS DE STATS (-6 A +6), CÁLCULO DE CRÍTICO E EVENTOS WWISE DE IMPACTO DE BATALHA

### 1. Mensagem de Coordenação e Sincronização do Time
Caro Codex, excelente trabalho na engenharia reversa do AArch64!
Para garantir total sincronia e **zero sobreposição de arquivos entre nós três**:
- **Codex (Seu Domínio Exclusivo):** Pasta `analysis/battle/*`
- **Claude Code:** Parsers de dados e tabelas de aprendizado (`learnset_table.*`, `tests/learnset_test.cpp`)
- **Antigravity:** Motor de execução nativo, integração gráfica NVN/GDI, HUD e mixer de áudio.

---

### 2. Objetivos Principais da Sua Nova Tarefa (~1 Hora)

#### A. Tabela de Estágios de Atributos (-6 a +6) e Fórmula de Crítico
Localizar no binário AArch64 descompactado (`analysis/boot/text.bin` e `rodata.bin`):
1. **Multiplicadores de Estágio de Stat (-6 a +6)**:
   - Tabela de frações ou float de -6 a +6 para Attack, Defense, SpAtk, SpDef, Speed:
     $$\text{Estágio } k \in [-6, 6] \implies \frac{\max(2, 2+k)}{\max(2, 2-k)}$$
     (-6: 2/8 = 0.25x, 0: 2/2 = 1.0x, +6: 8/2 = 4.0x).
   - Gerar `analysis/battle/stat_stages.h` com a tabela estática correspondente.
2. **Cálculo e Taxa de Golpe Crítico (Critical Hit)**:
   - Verificar a probabilidade base por estágio de crítico ($C_0 = 4.17\%$, $C_1 = 12.5\%$, $C_2 = 50\%$, $C_{\ge 3} = 100\%$).
   - Confirmar o multiplicador de dano crítico (1.5x no Trinity Engine, ignorando boosts defensivos do oponente).
   - Documentar em `analysis/battle/battle_formulas.md`.

---

#### B. Mapeamento de Eventos Wwise de Impacto em Combate (`PM_VOICE.bnk` / `SE.bnk`)
1. Identificar no Wwise HIRC os IDs de evento para feedback de impacto em combate:
   - Som de acerto normal (`PLAY_SE_BT_HIT_NORMAL` ou equivalente)
   - Som de super efetivo (`PLAY_SE_BT_HIT_SUPER` ou equivalente)
   - Som de pouco efetivo (`PLAY_SE_BT_HIT_NOTVERY` ou equivalente)
   - Som de acerto crítico (`PLAY_SE_BT_CRITICAL` ou equivalente)
2. Gerar `analysis/battle/battle_sfx_events.json` com os pares `{ "name": "...", "event_id": 0x... }`.

---

### 3. Entregas Esperadas
1. `analysis/battle/stat_stages.h`
2. `analysis/battle/battle_formulas.md`
3. `analysis/battle/battle_sfx_events.json`
