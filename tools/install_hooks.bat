@echo off
setlocal

cd /d "%~dp0\.."

set "HOOK_DIR=.git\hooks"
set "HOOK_FILE=%HOOK_DIR%\pre-commit"

if not exist ".git" (
    echo [ERRO] Diretorio .git nao encontrado na raiz do repositorio.
    exit /b 1
)

if not exist "%HOOK_DIR%" (
    mkdir "%HOOK_DIR%"
)

if exist "%HOOK_FILE%" (
    echo [AVISO] O hook pre-commit ja existe em "%HOOK_FILE%".
    echo Nao sobrescrevendo hook existente.
    exit /b 0
)

(
    echo #!/bin/sh
    echo python tools/check_repo_safety.py
    echo if [ $? -ne 0 ]; then
    echo     echo "Commit bloqueado: violacao de seguranca detectada por check_repo_safety.py"
    echo     exit 1
    echo fi
) > "%HOOK_FILE%"

echo [SUCESSO] Hook pre-commit instalado com sucesso em "%HOOK_FILE%".
exit /b 0
