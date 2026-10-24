@echo off
setlocal enabledelayedexpansion

set "PATH=%LOCALAPPDATA%\Microsoft\WinGet\Packages\BrechtSanders.WinLibs.POSIX.UCRT_Microsoft.Winget.Source_8wekyb3d8bbwe\mingw64\bin;%PATH%"

set RECOMP_STATUS=FALHOU
set ZA_PORT_STATUS=FALHOU

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
echo.
echo ========================================================
echo Resumo Final
echo ========================================================
echo recomp: %RECOMP_STATUS%
echo za_port: %ZA_PORT_STATUS%

if "%RECOMP_STATUS%"=="OK" (
    if "%ZA_PORT_STATUS%"=="OK" (
        exit /b 0
    )
)
exit /b 1
