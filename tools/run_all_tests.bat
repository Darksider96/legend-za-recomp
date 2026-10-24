@echo off
setlocal enabledelayedexpansion

rem Para preparar o venv para o --difftest:
rem python -m venv build\venv
rem build\venv\Scripts\pip install unicorn capstone

set "PATH=%LOCALAPPDATA%\Microsoft\WinGet\Packages\BrechtSanders.WinLibs.POSIX.UCRT_Microsoft.Winget.Source_8wekyb3d8bbwe\mingw64\bin;%PATH%"

set DO_DIFFTEST=0
if "%~1"=="--difftest" set DO_DIFFTEST=1

set RECOMP_STATUS=FALHOU
set ZA_PORT_STATUS=FALHOU
set DIFFTEST_STATUS=

echo ========================================================
echo [1/2] Compilando e testando: recomp
echo ========================================================
cmake -B build/recomp -S recomp -G Ninja -DCMAKE_BUILD_TYPE=Release
if errorlevel 1 goto after_recomp

cmake --build build/recomp
if errorlevel 1 goto after_recomp

ctest --test-dir build/recomp --output-on-failure
if errorlevel 1 goto after_recomp

set RECOMP_STATUS=OK

:after_recomp
echo.
echo ========================================================
echo [2/2] Compilando e testando: za_port
echo ========================================================
cmake -B build/za_port -S za_port -G Ninja -DCMAKE_BUILD_TYPE=Release
if errorlevel 1 goto after_za_port

cmake --build build/za_port
if errorlevel 1 goto after_za_port

ctest --test-dir build/za_port --output-on-failure
if errorlevel 1 goto after_za_port

set ZA_PORT_STATUS=OK

:after_za_port
if "%DO_DIFFTEST%"=="1" (
    echo.
    echo ========================================================
    echo [3/3] Teste diferencial: insn_difftest ^(Unicorn^)
    echo ========================================================
    if not "%RECOMP_STATUS%"=="OK" (
        set "DIFFTEST_STATUS=PULADO (recomp falhou)"
        echo difftest: !DIFFTEST_STATUS!
    ) else (
        if not exist build\venv\Scripts\python.exe (
            set "DIFFTEST_STATUS=PULADO (venv sem unicorn)"
            echo difftest: !DIFFTEST_STATUS!
        ) else (
            build\venv\Scripts\python.exe -I -c "import unicorn" >nul 2>&1
            if errorlevel 1 (
                set "DIFFTEST_STATUS=PULADO (venv sem unicorn)"
                echo difftest: !DIFFTEST_STATUS!
            ) else (
                build\venv\Scripts\python.exe -I recomp\tools\insn_difftest.py build\recomp exefs\main
                if errorlevel 1 (
                    set DIFFTEST_STATUS=FALHOU
                ) else (
                    set DIFFTEST_STATUS=OK
                )
            )
        )
    )
)

echo.
echo ========================================================
echo Resumo Final
echo ========================================================
echo recomp: !RECOMP_STATUS!
echo za_port: !ZA_PORT_STATUS!
if "!DO_DIFFTEST!"=="1" (
    echo difftest: !DIFFTEST_STATUS!
)

if "!RECOMP_STATUS!"=="OK" (
    if "!ZA_PORT_STATUS!"=="OK" (
        if "!DO_DIFFTEST!"=="1" (
            if "!DIFFTEST_STATUS!"=="FALHOU" exit /b 1
        )
        exit /b 0
    )
)
exit /b 1
