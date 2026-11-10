# すべての出力先・実行パスに最新の Resources を完全同期するスクリプト

$src = "project\Resources"
$destDirs = @(
    "generated\output\Debug\Resources",
    "generated\output\Release\Resources",
    "generated\output\Development\Resources",
    "project\x64\Debug\Resources",
    "project\x64\Release\Resources"
)

foreach ($dest in $destDirs) {
    if (Test-Path (Split-Path $dest -Parent)) {
        Write-Host "Syncing to $dest ..."
        robocopy $src $dest /E /NP /R:1 /W:1 /NJH /NJS
    }
}

Write-Host "Sync completed!"
