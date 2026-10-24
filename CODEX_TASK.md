# TAREFA PARA O CODEX — Catálogo de comandos IPC que o sdk envia, por interface

> **Tarefa NOVA.** A ficha de ABI (`r5_abi*`) foi aceita. Não edite arquivos de tarefas anteriores em
> `analysis/recomp/` (inclusive `kernel_census*`: as edições não pedidas na sua cópia não serão incorporadas).

## Contexto
O jogo recompilado já dá boot: o sdk recompilado conversa por IPC com serviços emulados no runtime
(`recomp/runtime/am.cpp`, `services.cpp`). Hoje cada comando que falta para o boot com o nome do serviço e o número
— um comando por rodada. Precisamos do catálogo **estático** dos comandos que o sdk envia em cada interface, para
implementar cada serviço inteiro de uma vez.

O cliente IPC do sdk é o `nn::sf` (CMIF). Cada método de proxy monta a mensagem no buffer de IPC da thread (TLS) com o
número do comando (palavra depois do `SFCI`, em `+8` do cabeçalho de dados) e o tamanho dos dados, e chama o
`SendSyncRequest`. Os proxies têm nomes nas exportações do sdk (`analysis/recomp/generated/sdk/exports.csv`, 23.000
símbolos) — por exemplo instâncias de templates `nn::sf::cmif::client::...` com a interface no nome
(`nn::am::service::ICommonStateGetter`, `nn::fssrv::sf::IFileSystemProxy`, `nn::hid::IHidServer`, ...). Confirme o
padrão real no binário.

Entradas: `analysis/recomp/generated/sdk/` (`image.bin`, `exports.csv`, `functions.csv`, `relocations.csv`,
`layout.csv`) e os CSVs do censo de kernel (`service_names.csv`). Capstone no `build\venv`.

## Arquivos que você pode criar (e SOMENTE estes)
- `analysis/recomp/sdk_ipc_commands.py`
- `analysis/recomp/sdk_ipc_commands_report.md`
- Saídas CSV **somente** em `analysis/recomp/generated/`
Regra de conteúdo do `.md`: nomes de interfaces/métodos (demangled), números de comando, tamanhos, RVAs, contagens.

## O que fazer
1. Descobrir como o sdk monta o número do comando (constante imediata gravada na mensagem, argumento de uma função
   comum de invocação, tabela de métodos...). Documente o padrão com 2–3 exemplos (só RVAs e mnemônicos).
2. Para cada método de proxy: interface, nome do método (se houver), número do comando, tamanho dos dados de entrada
   e de saída quando derivável, quantidade de objetos/handles de entrada e saída quando derivável, RVA.
3. Saída `generated/sdk_ipc_commands.csv` (`interface,metodo,comando,dados_entrada,dados_saida,objetos_saida,handles_saida,rva`).
4. No relatório: uma tabela por interface (ordenada por comando) e a lista de interfaces sem método identificado.

## Gabarito (o script deve conferir e imprimir): comandos observados no boot real
O catálogo **precisa** conter pelo menos estes (interface → comandos):
- `sm:` (TIPC) → 0 RegisterClient, 1 GetServiceHandle
- `lm` → 0 OpenLogger; `ILogger` → 0 Log, 1 SetDestination
- `appletOE` → 0 OpenApplicationProxy
- `IApplicationProxy` → 0, 1, 2, 3, 4, 10, 11, 20, 1000
- `ICommonStateGetter` → 0 GetEventHandle, 1 ReceiveMessage, 900
- `apm` → 0 OpenSession
Para cada item do gabarito ausente, explique no relatório por que o método não foi achado.

## Autoverificações
1. Gabarito acima (cada item encontrado ou explicado).
2. Nenhuma interface com dois métodos diferentes no mesmo número de comando (ou listar os conflitos).
3. Contagens do relatório batem com o CSV.

## Critério de aceite
```
build\venv\Scripts\python.exe -I analysis\recomp\sdk_ipc_commands.py
python tools\check_repo_safety.py
```

## Regras
- Se o mesmo erro acontecer duas vezes seguidas, **pare e relate**. Não tente de novo em loop.
- Não faça commit nem push.
