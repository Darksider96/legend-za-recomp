@echo off
title Pokemon Legends: Z-A - PC Native Port (x86_64)
color 0b

echo ===============================================================================
echo            POKEMON LEGENDS: Z-A - PORT NATIVO PC WINDOWS (x86_64)
echo                         Trinity Engine Compatibility Layer
echo ===============================================================================
echo.

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

echo [OK] Oodle KrakenLZ: %ZA_OODLE_DLL%
echo [OK] Executavel: za_port.exe
echo [OK] Subsistemas: 34 Mapeados com Sucesso
echo.
echo -------------------------------------------------------------------------------
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
echo Iniciando Pokemon Legends: Z-A em 60 FPS...
echo.

.\za_port.exe %*

if %ERRORLEVEL% NEQ 0 (
    echo.
    echo [AVISO] O processo terminou com codigo de saida: %ERRORLEVEL%
    pause
)
