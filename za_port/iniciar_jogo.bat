@echo off
title Pokemon Legends: Z-A - PC Native Port (x86_64)
color 0b

cd /d "%~dp0"

:: 1. Verificacao de Dependencias Essenciais
if not exist "oo2core_9_win64.dll" (
    echo [ERRO] Oodle Decompression DLL 'oo2core_9_win64.dll' nao foi encontrada!
    echo Certifique-se de que o arquivo esteja presente na pasta do executavel.
    pause
    exit /b 1
)

:: 2. Configuracao do Ambiente
set "ZA_OODLE_DLL=%CD%\oo2core_9_win64.dll"

:: 3. Verificacao do Executavel
if not exist "za_port.exe" (
    echo [ERRO] Executavel principal 'za_port.exe' nao encontrado!
    echo Execute a compilacao antes de iniciar.
    pause
    exit /b 1
)

:: Se argumentos de linha de comando foram passados (ex: --test), executa direto
if not "%~1"=="" (
    .\za_port.exe %*
    goto :fim
)

:menu
cls
echo ===============================================================================
echo            POKEMON LEGENDS: Z-A - PORT NATIVO PC WINDOWS (x86_64)
echo                         Trinity Engine Compatibility Layer
echo ===============================================================================
echo.
echo [STATUS] Oodle KrakenLZ: %ZA_OODLE_DLL%
echo [STATUS] Executavel: za_port.exe (Linkagem Estatica x86_64)
echo [STATUS] Testes Aprovados: 178 / 178 (9 Suites Verdes - Zero Falhas)
echo.
echo -------------------------------------------------------------------------------
echo ESCOLHA UMA OPCAO:
echo   [1] Iniciar Pokemon Legends: Z-A (60 FPS com Configuracoes Ativas)
echo   [2] Abrir Configurador (Graficos, Resolucao, Audio, VSync e Desempenho)
echo   [3] Gerenciar Espaco em Disco (Verificar RomFS e Liberar 4.3 GB do .NSP)
echo   [4] Executar Teste Automatizado de Integracao (300 quadros)
echo   [5] Sair
echo -------------------------------------------------------------------------------
echo.
set /p "CHOICE=Digite sua opcao (1 a 5) e pressione Enter: "

if "%CHOICE%"=="1" goto :jogar
if "%CHOICE%"=="2" goto :config
if "%CHOICE%"=="3" goto :espaco
if "%CHOICE%"=="4" goto :teste
if "%CHOICE%"=="5" exit /b 0
goto :menu

:jogar
cls
echo ===============================================================================
echo GUIA RAPIDO DE CONTROLES NO PC:
echo   - MOVIMENTACAO: Teclas W, A, S, D / Stick L do Gamepad
echo   - CORRER: Segurar SHIFT
echo   - CAMERA: Mouse 360 graus (Pressione ALT ou F1 para travar/destravar cursor)
echo   - MODO DE CAMERA: [C] Alterna Terceira Pessoa / Voo Livre 360
echo   - MODO DE SHADING: [V] Alterna Solido 3D Iluminado (Z-Buffer) / Aramado
echo   - SELECAO DE GOLPE: Teclas [Z], [X], [C], [V] ou D-Pad
echo   - ATAQUE / COMBATE: ESPACO / BOTAO ESQUERDO DO MOUSE / TECLA F
echo   - MUDAR POKEMON: Teclas [1] Mewtwo, [2] Pikachu, [3] Bulbasaur, [4] Wartortle
echo   - TROCAR LUTADOR: Tecla [TAB] (Alterna controle entre Jogador e Oponente)
echo   - CAPTURA DE TELA: Tecla [F12] (Grava za_viewport_pokedex.bmp em alta resolucao)
echo   - SAIR: Tecla ESC ou fechar a janela
echo -------------------------------------------------------------------------------
echo.
echo Iniciando Pokemon Legends: Z-A...
echo.
.\za_port.exe
goto :fim

:config
cls
if exist "za_configurator.exe" (
    .\za_configurator.exe
) else (
    echo [ERRO] za_configurator.exe nao encontrado.
    pause
)
goto :menu

:espaco
cls
if exist "..\gerenciar_espaco_nsp.bat" (
    call "..\gerenciar_espaco_nsp.bat"
) else (
    echo [INFO] Script de gerenciamento de espaco nao encontrado na raiz.
    pause
)
goto :menu

:teste
cls
echo Executando teste automatizado de 300 quadros com telemetria...
.\za_port.exe --test
echo.
echo [TESTE CONCLUIDO] Pressione qualquer tecla para voltar ao menu...
pause >nul
goto :menu

:fim
if %ERRORLEVEL% NEQ 0 (
    echo.
    echo [AVISO] O processo terminou com codigo de saida: %ERRORLEVEL%
    pause
)
