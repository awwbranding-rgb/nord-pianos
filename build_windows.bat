@echo off
setlocal enabledelayedexpansion
cd /d "%~dp0"

echo =======================================================
echo          NPno Piano Reader - VST3 x64 Build
echo =======================================================

where cmake >nul 2>&1 || (echo ERRO: CMake nao encontrado no PATH.& pause & exit /b 1)
where git >nul 2>&1 || (echo ERRO: Git nao encontrado no PATH.& pause & exit /b 1)

:: Detectar gerador Visual Studio
set "VS_GENERATOR=Visual Studio 17 2022"
reg query "HKLM\SOFTWARE\Microsoft\VisualStudio\SxS\VS7" /v "18.0" >nul 2>&1
if %errorlevel% equ 0 (
    set "VS_GENERATOR=Visual Studio 18 2026"
)

echo Usando gerador CMake: %VS_GENERATOR%
echo.

if not exist build-win mkdir build-win

echo [1/3] Configurando projeto com JUCE 8 (FetchContent)...
cmake -S . -B build-win -G "%VS_GENERATOR%" -A x64 -DBUILD_PLUGIN=ON
if %errorlevel% neq 0 (
    echo.
    echo Tentando com o gerador padrao do CMake...
    cmake -S . -B build-win -A x64 -DBUILD_PLUGIN=ON
    if %errorlevel% neq 0 (
        echo ERRO: Falha na configuracao do CMake.
        pause
        exit /b 1
    )
)

echo.
echo [2/3] Compilando VST3 em Release x64...
cmake --build build-win --config Release --parallel 4
if %errorlevel% neq 0 (
    echo ERRO: Falha na compilacao do plugin.
    pause
    exit /b 1
)

set "ARTIFACT_PATH=build-win\NordPianoPrototype_artefacts\Release\VST3\NPno Piano Reader.vst3"

if not exist "%ARTIFACT_PATH%" (
    echo ERRO: O arquivo VST3 nao foi encontrado no caminho esperado:
    echo %ARTIFACT_PATH%
    pause
    exit /b 1
)

echo.
echo [3/3] Instalando e organizando arquivos...

:: Copiar para C:\Program Files\Common Files\VST3
if exist "C:\Program Files\Common Files\VST3" (
    echo Instalando em C:\Program Files\Common Files\VST3...
    xcopy /E /I /Y "%ARTIFACT_PATH%" "C:\Program Files\Common Files\VST3\NPno Piano Reader.vst3" >nul 2>&1
    if !errorlevel! equ 0 (
        echo Plugin instalado com sucesso no diretorio do sistema!
    ) else (
        echo Aviso: Nao foi possivel copiar diretamente para Common Files\VST3 (requer administrador).
    )
)

:: Copiar para pacote final de entrega
set "FINAL_DIR=..\Nord_Piano_Reader_Final"
if not exist "%FINAL_DIR%" mkdir "%FINAL_DIR%"
xcopy /E /I /Y "%ARTIFACT_PATH%" "%FINAL_DIR%\NPno Piano Reader.vst3" >nul 2>&1

echo.
echo =======================================================
echo          COMPILACAO E INSTALACAO CONCLUIDAS!
echo =======================================================
echo.
echo Plugin VST3 gerado:
echo   %ARTIFACT_PATH%
echo.
echo No Ableton Live:
echo   1. Va em Preferencias ^> Plug-Ins
echo   2. Ative 'Usar pasta de plug-ins de sistema VST3'
echo   3. Clique em 'Escanear novamente plug-ins' (Rescan)
echo   4. O plugin aparecera em Plug-Ins ^> VST3 ^> NPno Piano Reader!
echo.
pause
