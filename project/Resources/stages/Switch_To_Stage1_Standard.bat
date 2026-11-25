@echo off
chcp 65001 >nul
set "RES=%~dp0Resources"
if not exist "%RES%\Stage1_Standard\stage_layout.json" (
    echo [ERROR] Resources\Stage1_Standard not found.
    pause
    exit /b 1
)
copy /Y "%RES%\Stage1_Standard\*" "%RES%\" >nul
echo ============================================================
echo  [STAGE 1 RESTORED] STANDARD COMPOUND RAID (57m x 75m)
echo  元の完成ステージ (Stage 1) に戻しました。
echo ============================================================
pause

