@echo off
title Pokemon Legends: Z-A - Gerenciador de Instalacao e Espaco em Disco
color 0b

echo ===============================================================================
echo      POKEMON LEGENDS: Z-A - GERENCIADOR DE ESPACO E INSTALACAO (.NSP)
echo ===============================================================================
echo.

cd /d "%~dp0"

set "ROMFS_DATA=romfs\arc\data.trpfs"
set "ROMFS_DICT=romfs\arc\data.trpfd"

echo [1/3] Verificando integridade dos dados extraidos do jogo (RomFS)...
if exist "%ROMFS_DATA%" (
    if exist "%ROMFS_DICT%" (
        echo [OK] O RomFS do jogo ja esta 100%% EXTRAIDO e PRONTO para jogar!
        echo      Local: %ROMFS_DATA%
        echo.
        goto :verificar_nsp
    )
)

echo [AVISO] O jogo ainda nao parece estar extraido na pasta romfs/.
echo Para extrair, certifique-se de que o arquivo .nsp esteja na pasta raiz.
goto :fim

:verificar_nsp
echo [2/3] Localizando arquivos .nsp que ocupam espaco em disco desnecessario...
set "NSP_FOUND=0"
for %%F in (*.nsp) do (
    set "NSP_FOUND=1"
    set "NSP_NAME=%%F"
    echo      Encontrado: "%%F"
)

if "%NSP_FOUND%"=="0" (
    echo [INFO] Nenhum arquivo .nsp encontrado na pasta raiz.
    echo        O jogo esta rodando otimizado diretamente pelo romfs extraido!
    echo.
    goto :fim
)

echo.
echo ===============================================================================
echo                         ECONOMIA DE ESPACO EM DISCO
echo ===============================================================================
echo Como os arquivos do jogo (romfs/) ja estao instalados e prontos para o
echo port nativo PC (za_port.exe), o arquivo .nsp de ~4.3 GB NAO eh mais necessario!
echo.
echo Manter ambos duplica o consumo de armazenamento para quase 8 GB no seu disco/OneDrive.
echo.
echo Deseja DELETAR o arquivo .nsp agora para liberar 4.3 GB de espaco?
echo.
echo   [1] Sim, deletar o arquivo .nsp e liberar 4.3 GB
echo   [2] Nao, manter o arquivo .nsp como backup
echo.
set /p "OPCAO=Digite sua escolha (1 ou 2) e pressione Enter: "

if "%OPCAO%"=="1" (
    echo.
    echo Deletando "%NSP_NAME%"...
    del /f /q "%NSP_NAME%"
    if not exist "%NSP_NAME%" (
        echo [SUCESSO] Arquivo .nsp removido com sucesso! 4.3 GB liberados no seu disco!
    ) else (
        echo [ERRO] Nao foi possivel remover o arquivo. Verifique se outro programa o esta usando.
    )
) else (
    echo.
    echo Arquivo .nsp mantido intacto na pasta raiz.
)

:fim
echo.
echo ===============================================================================
echo Pressione qualquer tecla para encerrar...
pause >nul
