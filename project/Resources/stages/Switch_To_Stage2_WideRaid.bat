@echo off
chcp 65001 >nul
set "RES=%~dp0Resources"
if not exist "%RES%\Stage2_Wide\stage_layout.json" (
    echo [ERROR] Resources\Stage2_Wide not found.
    pause
    exit /b 1
)
copy /Y "%RES%\Stage2_Wide\*" "%RES%\" >nul
echo ============================================================
echo  [STAGE 2 ACTIVATED] OPERATION: NORTHERN EXPANSE (243m x 268m)
echo  DirectXGame.exe を起動すると広域サバイバルステージで遊べます！
echo ============================================================
pause

