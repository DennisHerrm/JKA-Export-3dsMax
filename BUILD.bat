@echo off
REM ================================================================
REM  BUILD.bat - JKA Export bauen und installieren.
REM  Inkrementell, fuer jeden Max-Jahrgang in JAHRGAENGE (2016 bis 2027).
REM    BUILD.log  - Bau und Installation
REM  Mit einem Jahrgang als Parameter nur diesen:  BUILD.bat 2027
REM  Ohne Installation:                            set JKAEXP_NUR_BAUEN=1
REM ================================================================
setlocal enabledelayedexpansion
cd /d "%~dp0"
set "JAHRGAENGE=2016 2017 2018 2019 2020 2021 2022 2023 2024 2025 2026 2027"
set "LOG=%~dp0BUILD.log"
set "OUTPUT=%~dp0output"
set "VSWHERE=%ProgramFiles(x86)%\Microsoft Visual Studio\Installer\vswhere.exe"
set "CMAKE="
set "GEN=Visual Studio 17 2022"
set "GEBAUT=0"
set "NUR=%~1"
echo BUILD.bat  %DATE% %TIME%> "%LOG%"
echo.
echo ================================================================
echo   JKA Export - bauen und installieren
echo ================================================================

where cmake >nul 2>&1
if errorlevel 1 goto suche_vs_cmake
set "CMAKE=cmake"
goto habe_cmake
:suche_vs_cmake
if not exist "%VSWHERE%" goto kein_cmake
for /f "usebackq tokens=*" %%i in (`"%VSWHERE%" -latest -products * -property installationPath 2^>nul`) do call :pruefe_cmake "%%i"
if "%CMAKE%"=="" goto kein_cmake
goto habe_cmake
:pruefe_cmake
if exist "%~1\Common7\IDE\CommonExtensions\Microsoft\CMake\CMake\bin\cmake.exe" set "CMAKE=%~1\Common7\IDE\CommonExtensions\Microsoft\CMake\CMake\bin\cmake.exe"
goto :eof
:kein_cmake
echo KEIN CMAKE GEFUNDEN - Visual Studio mit C++ und CMake installieren.
goto ende

:habe_cmake
if not exist "%VSWHERE%" goto generator_fertig
for /f "usebackq tokens=*" %%i in (`"%VSWHERE%" -version [18.0^,19.0^) -property installationPath 2^>nul`) do set "GEN=Visual Studio 18 2026"
:generator_fertig

echo [1/2] Plugin bauen ...
if not "%NUR%"=="" goto nur_einer
for %%V in (%JAHRGAENGE%) do call :baue_version %%V
goto nach_bau
:nur_einer
call :baue_version %NUR%
:nach_bau
if "%GEBAUT%"=="0" goto nichts
if "%JKAEXP_NUR_BAUEN%"=="1" goto fertig

echo.
echo [2/2] Installieren ...
set "JKAEXP_PAUSE_VORHER=%JKAEXP_KEIN_PAUSE%"
set JKAEXP_KEIN_PAUSE=1
call "%~dp0INSTALLIERE.bat"
set "JKAEXP_KEIN_PAUSE=%JKAEXP_PAUSE_VORHER%"
:fertig
echo.
echo Fertig: %GEBAUT% Jahrgang(Jahrgaenge) gebaut.
echo Protokoll: %LOG%
goto ende

:baue_version
set "VER=%~1"
set "SDK=C:\Program Files\Autodesk\3ds Max %VER% SDK\maxsdk"
if not exist "%SDK%\include\max.h" (
  echo       Max %VER%: kein SDK unter %SDK%
  goto :eof
)
echo       Max %VER% ...
if exist "build_%VER%\CMakeCache.txt" goto nur_bauen_version
"%CMAKE%" -S . -B "build_%VER%" -G "%GEN%" -A x64 -D3DSMAX_SDK_DIR="%SDK%" -DMAX_VERSION=%VER% >> "%LOG%" 2>&1
if errorlevel 1 goto :baufehler_version
:nur_bauen_version
"%CMAKE%" --build "build_%VER%" --config Release >> "%LOG%" 2>&1
if errorlevel 1 goto :baufehler_version
if not exist "build_%VER%\bin\JkaExport.dle" goto :baufehler_version
mkdir "%OUTPUT%\%VER%" >nul 2>&1
copy /Y "build_%VER%\bin\JkaExport.dle" "%OUTPUT%\%VER%\JkaExport.dle" >nul 2>&1
if not exist "%OUTPUT%\%VER%\JkaExport.dle" goto :baufehler_version
echo             ok -^> output\%VER%\JkaExport.dle
set /a GEBAUT+=1
goto :eof

:baufehler_version
if exist "%OUTPUT%\%VER%\JkaExport.dle" del /q "%OUTPUT%\%VER%\JkaExport.dle"
echo             FEHLGESCHLAGEN - die Fehlerzeilen:
findstr /R /C:"error C" /C:"error LNK" /C:"error MSB" /C:"error RC" "%LOG%" 2>nul
echo             Vollstaendiges Protokoll: %LOG%
goto :eof

:nichts
echo NICHTS GEBAUT - siehe %LOG%
goto ende

:ende
echo.
if not "%JKAEXP_KEIN_PAUSE%"=="1" if not "%JKAEXP_NUR_BAUEN%"=="1" pause
endlocal
