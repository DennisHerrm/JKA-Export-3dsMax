@echo off
REM ===========================================================================
REM  INSTALLIERE.bat - das gebaute Plugin ins Paket legen und installieren.
REM
REM  Wie beim SWBF2 Import: 3ds Max durchsucht
REM    1. %ALLUSERSPROFILE%\Autodesk\ApplicationPlugins\<name>   (alle Benutzer)
REM    2. %APPDATA%\Autodesk\ApplicationPlugins\<name>           (ohne Adminrechte)
REM  Die PackageContents.xml muss DIREKT im Paketordner liegen.
REM ===========================================================================
setlocal enabledelayedexpansion
cd /d "%~dp0"
set "OUTPUT=%~dp0output"
set "PKG=%~dp0package\JkaExport"
set "ZIEL_ALLE=%ALLUSERSPROFILE%\Autodesk\ApplicationPlugins\JkaExport"
set "ZIEL_ICH=%APPDATA%\Autodesk\ApplicationPlugins\JkaExport"
set "DEST="
set "ILOG=%~dp0INSTALLIERE.log"
if /i "%~1"=="/erhoeht" set JKAEXP_ERHOEHT=1
if defined JKAEXP_ERHOEHT (echo --- erhoehter Lauf %DATE% %TIME%>> "%ILOG%") else (echo INSTALLIERE.bat %DATE% %TIME%> "%ILOG%")

if not exist "%OUTPUT%" goto kein_output
if not exist "%PKG%\PackageContents.xml" goto kein_paket

mkdir "%ALLUSERSPROFILE%\Autodesk\ApplicationPlugins" >nul 2>&1
set "PROBE=%ALLUSERSPROFILE%\Autodesk\ApplicationPlugins\.jkaprobe"
2>nul (>>"%PROBE%" (call )) && set "DEST=%ZIEL_ALLE%"
del "%PROBE%" >nul 2>&1
if defined DEST goto ziel_steht
if defined JKAEXP_ERHOEHT goto ausweichen
echo Fuer C:\ProgramData fehlen die Rechte - ich starte als Administrator neu ...
powershell -NoProfile -Command "Start-Process -FilePath '%~f0' -ArgumentList '/erhoeht' -Verb RunAs -Wait" >nul 2>&1
if errorlevel 1 goto ausweichen
echo Der erhoehte Lauf ist beendet - Ergebnis in INSTALLIERE.log.
goto ende
:ausweichen
echo Ohne Adminrechte - Ausweichort AppData (nur dieser Benutzer).
set "DEST=%ZIEL_ICH%"
:ziel_steht
echo Ziel: %DEST%
>>"%ILOG%" echo Ziel: %DEST%

set GESPERRT=
if exist "%DEST%" for /R "%DEST%" %%f in (JkaExport.dl*) do if /i "%%~xf"==".dle" call :pruefe_sperre "%%f"
if defined GESPERRT goto gesperrt

if exist "%PKG%\Contents" rmdir /s /q "%PKG%\Contents"
mkdir "%PKG%\Contents" >nul 2>&1
set GEFUNDEN=0
for %%V in (2016 2017 2018 2019 2020 2021 2022 2023 2024 2025 2026 2027) do call :packe %%V
if "%GEFUNDEN%"=="0" goto nichts_gebaut
mkdir "%PKG%\Contents\MacroScripts" >nul 2>&1
mkdir "%PKG%\Contents\Pre-Start-Up_Scripts" >nul 2>&1
mkdir "%PKG%\Contents\Post-Start-Up_Scripts" >nul 2>&1
copy /Y "%~dp0scripts\JkaMenu_2016_2024.ms" "%PKG%\Contents\Post-Start-Up_Scripts\" >nul 2>&1
copy /Y "%~dp0scripts\JkaExport.mcr" "%PKG%\Contents\MacroScripts\" >nul 2>&1
copy /Y "%~dp0scripts\JkaMenu_2025_2027.ms" "%PKG%\Contents\Pre-Start-Up_Scripts\" >nul 2>&1

if exist "%DEST%\Contents" rmdir /s /q "%DEST%\Contents"
xcopy /E /I /Y /Q "%PKG%" "%DEST%" >nul
if errorlevel 1 goto kopierfehler
if /i "%DEST%"=="%ZIEL_ALLE%" if exist "%ZIEL_ICH%\PackageContents.xml" rmdir /s /q "%ZIEL_ICH%" >nul 2>&1
if not exist "%DEST%\PackageContents.xml" goto kopierfehler
>>"%ILOG%" echo FERTIG: %GEFUNDEN% .dle in %DEST%
echo FERTIG: %GEFUNDEN% Max-Fassung(en) in %DEST%
echo In 3ds Max (neu starten): Menue "JKA Tool" -^> "Export GLM/GLA"
goto ende

:packe
if not exist "%OUTPUT%\%~1\JkaExport.dle" goto :eof
mkdir "%PKG%\Contents\%~1" >nul 2>&1
copy /Y "%OUTPUT%\%~1\JkaExport.dle" "%PKG%\Contents\%~1\JkaExport.dle" >nul 2>&1
set /a GEFUNDEN+=1
goto :eof

:pruefe_sperre
if not exist "%~1" goto :eof
2>nul (>>"%~1" (call )) || set "GESPERRT=%~1"
goto :eof

:gesperrt
>>"%ILOG%" echo FEHLER: gesperrt (Max laeuft noch): %GESPERRT%
echo FEHLER: %GESPERRT% ist gesperrt - 3ds Max schliessen und erneut starten.
goto ende
:kopierfehler
>>"%ILOG%" echo FEHLER beim Kopieren nach %DEST%
echo FEHLER beim Kopieren nach %DEST%
goto ende
:kein_output
echo FEHLER: output\ fehlt - zuerst BUILD.bat.
goto ende
:nichts_gebaut
echo FEHLER: keine JkaExport.dle in output\.
goto ende
:kein_paket
echo FEHLER: package\JkaExport\PackageContents.xml fehlt.
goto ende
:ende
if defined JKAEXP_KEIN_PAUSE goto raus
pause >nul
:raus
endlocal
