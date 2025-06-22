@echo off
setlocal enabledelayedexpansion

set "parent_dir=%~dp0"
set "x86_dir=%parent_dir%x86"
set "x64_dir=%parent_dir%x64"

if not exist "%x86_dir%" (
    echo Le dossier x86 est introuvable.
    pause
    exit /b 1
)

if not exist "%x64_dir%" (
    echo Le dossier x64 est introuvable.
    pause
    exit /b 1
)

copy "%x86_dir%\XOrderInjector.exe" "%parent_dir%XOrderInjector_x86.exe"
if errorlevel 1 (
    echo Erreur lors de la copie de XOrderInjector.exe depuis le dossier x86.
    pause
    exit /b 1
)

copy "%x86_dir%\XOrderHook.dll" "%parent_dir%XOrderHook_x86.dll"
if errorlevel 1 (
    echo Erreur lors de la copie de XOrderHook.dll depuis le dossier x86.
    pause
    exit /b 1
)

copy "%x64_dir%\XOrderInjector.exe" "%parent_dir%XOrderInjector.exe"
if errorlevel 1 (
    echo Erreur lors de la copie de XOrderInjector.exe depuis le dossier x64.
    pause
    exit /b 1
)

copy "%x64_dir%\XOrderHook.dll" "%parent_dir%XOrderHook.dll"
if errorlevel 1 (
    echo Erreur lors de la copie de XOrderHook.dll depuis le dossier x64.
    pause
    exit /b 1
)

copy "%x64_dir%\XOrderHook.dll" "%parent_dir%SLD3.dll"
if errorlevel 1 (
    echo Erreur lors de la copie de SLD3.dll depuis le dossier x64.
    pause
    exit /b 1
)

echo Fichiers copiés avec succès.

exit
