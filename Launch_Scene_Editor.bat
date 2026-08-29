@echo off
chcp 65001 > nul
title Duckov Scene Config Editor Launcher
echo ========================================================
echo   🎖️ Duckov Tactical Scene Config Editor を起動しています...
echo ========================================================

py project/tools/scene_editor.py
if %ERRORLEVEL% EQU 0 exit /b 0

python project/tools/scene_editor.py
if %ERRORLEVEL% EQU 0 exit /b 0

echo.
echo [ERROR] Python の実行に失敗しました。
echo py または python がインストールされているかご確認ください。
pause
