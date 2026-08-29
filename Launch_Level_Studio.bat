@echo off
chcp 65001 > nul
title Duckov Stage Studio Launcher
echo ========================================================
echo   🎖️ Duckov Stage Studio (Level & Object Editor) を起動しています...
echo ========================================================

py project/tools/level_studio.py
if %ERRORLEVEL% EQU 0 exit /b 0

python project/tools/level_studio.py
if %ERRORLEVEL% EQU 0 exit /b 0

echo.
echo [ERROR] Python の実行に失敗しました。
echo py または python がインストールされているかご確認ください。
pause
