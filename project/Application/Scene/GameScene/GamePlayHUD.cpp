#include "GamePlayHUD.h"
#include "TextureManager.h"
#include "Camera.h"
#include "../../Player/Player.h"
#include "../../Enemy/Enemy.h"
#include "../../Enemy/MovingEnemy.h"
#include "Obstacle.h"
#include "Target.h"
#include "RaidStats.h"
#include "TutorialSign.h"
#include <cmath>
#include <algorithm>

#ifdef USE_IMGUI
#include "imgui.h"
#include "imgui_internal.h"
#endif

void GamePlayHUD::Initialize()
{
    medkitTextureIndex_   = TextureManager::GetInstance()->Load("Resources/item_medkit.jpg");
    ammoTextureIndex_     = TextureManager::GetInstance()->Load("Resources/item_ammo.jpg");
    goldDuckTextureIndex_ = TextureManager::GetInstance()->Load("Resources/item_gold_duck.jpg");
    roublesTextureIndex_  = TextureManager::GetInstance()->Load("Resources/item_roubles.jpg");
}

bool GamePlayHUD::Project3DTo2D(const Vector3& pos3D, const Matrix4x4& vp, float screenW, float screenH, Vector2& outPos)
{
    float w = pos3D.x * vp.m[0][3] + pos3D.y * vp.m[1][3] + pos3D.z * vp.m[2][3] + vp.m[3][3];
    if (w <= 0.0f) return false;
    float x = (pos3D.x * vp.m[0][0] + pos3D.y * vp.m[1][0] + pos3D.z * vp.m[2][0] + vp.m[3][0]) / w;
    float y = (pos3D.x * vp.m[0][1] + pos3D.y * vp.m[1][1] + pos3D.z * vp.m[2][1] + vp.m[3][1]) / w;
    outPos.x = (x + 1.0f) * 0.5f * screenW;
    outPos.y = (1.0f - y) * 0.5f * screenH;
    return true;
}

void GamePlayHUD::Draw(const GamePlayHUDContext& ctx, float deltaTime)
{
#ifdef USE_IMGUI
    ImGuiIO& io = ImGui::GetIO();
    float screenW = io.DisplaySize.x;
    float screenH = io.DisplaySize.y;
    if (screenW <= 0.0f || screenH <= 0.0f) return;

    // 各種HUDレイヤーの順番描画
    DrawLowHpRedVignetteEffect(ctx);
    DrawLaserSight(ctx);
    DrawDamageIndicator(ctx);
    DrawFloatingTexts(ctx);
    DrawVisionConesAndGizmos(ctx);
    DrawLootingHUD(ctx);
    DrawMissionObjectiveHUD(ctx);
    DrawTutorialSignHUD(ctx);
    DrawPlayerAmmoHUD(ctx, deltaTime);
    DrawDeathSequenceHUD(ctx);
    DrawPerformanceTrackerUI(ctx, deltaTime);

    // シーン入場時のスムーズなフェードイン演出 (0.75秒)
    if (ctx.sceneEntranceFadeTimer > 0.0f)
    {
        ImGuiIO& io = ImGui::GetIO();
        ImDrawList* drawList = ImGui::GetForegroundDrawList();
        float screenW = io.DisplaySize.x;
        float screenH = io.DisplaySize.y;
        if (drawList && screenW > 0.0f && screenH > 0.0f)
        {
            float alpha = (std::clamp)(ctx.sceneEntranceFadeTimer / 0.75f, 0.0f, 1.0f);
            int aVal = static_cast<int>(255.0f * alpha);
            drawList->AddRectFilled(ImVec2(0, 0), ImVec2(screenW, screenH), IM_COL32(8, 14, 20, aVal));
        }
    }
#endif
}

// =============================================================================
// 作戦目標 & レイドタイマー HUD (画面上部中央)
// =============================================================================

void GamePlayHUD::DrawMissionObjectiveHUD(const GamePlayHUDContext& ctx)
{
#ifdef USE_IMGUI
    if (!ctx.player || ctx.player->IsDead() || ctx.isDeathSequenceActive) return;

    ImGuiIO& io = ImGui::GetIO();
    ImDrawList* drawList = ImGui::GetForegroundDrawList();
    float screenW = io.DisplaySize.x;
    float screenH = io.DisplaySize.y;

    int totalTargets = 0;
    int destroyedCount = 0;
    if (ctx.targets)
    {
        totalTargets = static_cast<int>(ctx.targets->size());
        for (const auto& t : *ctx.targets)
        {
            if (t && t->IsDead()) destroyedCount++;
        }
    }

    float distToExtract = 0.0f;
    if (ctx.player)
    {
        Vector3 diff = ctx.extractionGoalPos - ctx.player->GetPosition();
        distToExtract = std::sqrt(diff.x * diff.x + diff.z * diff.z);
    }

    // 作戦進行バー (幅560px / 高さ34px)
    float hudW = 560.0f;
    float hudH = 34.0f;
    float hudX = (screenW - hudW) * 0.5f;
    float hudY = 12.0f;

    ImVec2 hMin(hudX, hudY);
    ImVec2 hMax(hudX + hudW, hudY + hudH);

    drawList->AddRectFilled(hMin, hMax, IM_COL32(12, 16, 20, 210), 6.0f);

    float remTime = RaidStats::GetInstance().GetRemainingTime();
    int remMin = static_cast<int>(remTime) / 60;
    int remSec = static_cast<int>(remTime) % 60;

    ImU32 borderCol = ctx.isReadyToExtract ? IM_COL32(0, 255, 140, 240) : IM_COL32(80, 110, 130, 180);
    if (remTime < 30.0f)
    {
        static float tBlink = 0.0f;
        tBlink += io.DeltaTime * 8.0f;
        borderCol = (std::sin(tBlink) > 0.0f) ? IM_COL32(255, 40, 40, 255) : IM_COL32(120, 20, 20, 200);
    }
    else if (remTime < 60.0f)
    {
        borderCol = IM_COL32(255, 160, 30, 220);
    }
    drawList->AddRect(hMin, hMax, borderCol, 6.0f, 0, 1.5f);

    char objBarText[256];
    if (ctx.isReadyToExtract)
    {
        sprintf_s(objBarText, "⏱️ %02d:%02d  |  [OK] 標的: %d/%d [完了]  |  [EXTRACT] 脱出地点: 開放中! (%.0fm)", remMin, remSec, destroyedCount, totalTargets, distToExtract);
        drawList->AddText(ImVec2(hudX + 14.0f, hudY + 8.0f), IM_COL32(0, 255, 160, 255), objBarText);
    }
    else
    {
        sprintf_s(objBarText, "⏱️ %02d:%02d  |  [TARGET] 標的破壊: %d/%d 体  |  [LOCKED] 脱出地点: ロック中", remMin, remSec, destroyedCount, totalTargets);
        ImU32 txtCol = (remTime < 60.0f) ? IM_COL32(255, 200, 60, 255) : IM_COL32(230, 240, 245, 240);
        drawList->AddText(ImVec2(hudX + 14.0f, hudY + 8.0f), txtCol, objBarText);
    }
#endif
}

// =============================================================================
// タクティカル インベントリ ＆ 弾薬・回復HUD (画面右下)
// =============================================================================

void GamePlayHUD::DrawPlayerAmmoHUD(const GamePlayHUDContext& ctx, float deltaTime)
{
#ifdef USE_IMGUI
    if (!ctx.player || ctx.player->IsDead() || ctx.isDeathSequenceActive) return;

    ImGuiIO& io = ImGui::GetIO();
    ImDrawList* drawList = ImGui::GetForegroundDrawList();
    float screenW = io.DisplaySize.x;
    float screenH = io.DisplaySize.y;

    int ammo = ctx.player->GetMagazineAmmo();
    int maxAmmo = ctx.player->GetMaxMagazineAmmo();
    int resAmmo = ctx.player->GetReserveAmmo();
    bool isReloading = ctx.player->IsReloading();
    float reloadProgress = ctx.player->GetReloadProgress();
    bool isCancelled = (ctx.player->GetReloadCancelledTimer() > 0.0f);

    float panelW = 280.0f;
    float panelH = 118.0f;
    float panelX = screenW - panelW - 20.0f;
    float panelY = screenH - panelH - 20.0f;

    ImVec2 pMin(panelX, panelY);
    ImVec2 pMax(panelX + panelW, panelY + panelH);

    drawList->AddRectFilled(pMin, pMax, IM_COL32(12, 16, 22, 235), 8.0f);

    ImU32 borderCol = IM_COL32(60, 90, 110, 180);
    if (isReloading) borderCol = IM_COL32(0, 255, 140, 220);
    else if (ctx.player->IsHealing()) borderCol = IM_COL32(0, 255, 180, 255);
    else if (isCancelled)
    {
        static float cBlink = 0.0f;
        cBlink += deltaTime * 10.0f;
        borderCol = (std::sin(cBlink) > 0.0f) ? IM_COL32(255, 140, 30, 255) : IM_COL32(200, 50, 20, 220);
    }
    else if (ammo == 0) borderCol = IM_COL32(255, 40, 40, 240);
    else if (ammo <= 6) borderCol = IM_COL32(255, 160, 20, 220);
    drawList->AddRect(pMin, pMax, borderCol, 8.0f, 0, 1.5f);

    // 1. 弾薬アイコン ＆ 残弾数
    float iconSize = 28.0f;
    float row1Y = panelY + 8.0f;
    if (ammoTextureIndex_ != UINT32_MAX)
    {
        uint64_t ammoGpu = TextureManager::GetInstance()->GetSrvHandleGPU(ammoTextureIndex_).ptr;
        if (ammoGpu != 0)
        {
            drawList->AddImageRounded((ImTextureID)ammoGpu, ImVec2(panelX + 10.0f, row1Y), ImVec2(panelX + 10.0f + iconSize, row1Y + iconSize), ImVec2(0, 0), ImVec2(1, 1), IM_COL32(255, 255, 255, 255), 4.0f);
            drawList->AddRect(ImVec2(panelX + 10.0f, row1Y), ImVec2(panelX + 10.0f + iconSize, row1Y + iconSize), IM_COL32(80, 120, 140, 200), 4.0f, 0, 1.0f);
        }
    }

    char ammoStr[64];
    float textOffsetX = panelX + 44.0f;
    if (isReloading)
    {
        sprintf_s(ammoStr, "RELOAD %d%%  [RES: %d]", static_cast<int>(reloadProgress * 100.0f), resAmmo);
        drawList->AddText(ImVec2(textOffsetX, row1Y), IM_COL32(0, 255, 140, 255), ammoStr);
    }
    else if (isCancelled)
    {
        sprintf_s(ammoStr, "%2d/%d [キャンセル!] (予備:%d)", ammo, maxAmmo, resAmmo);
        drawList->AddText(ImVec2(textOffsetX, row1Y), IM_COL32(255, 150, 40, 255), ammoStr);
    }
    else
    {
        sprintf_s(ammoStr, "AMMO  %2d / %d  [RES: %d]", ammo, maxAmmo, resAmmo);
        ImU32 numCol = IM_COL32(240, 240, 245, 255);
        if (ammo == 0) numCol = IM_COL32(255, 60, 60, 255);
        else if (ammo <= 6) numCol = IM_COL32(255, 180, 40, 255);
        drawList->AddText(ImVec2(textOffsetX, row1Y), numCol, ammoStr);
    }

    // 弾薬ゲージバー
    float barX = textOffsetX;
    float barY = row1Y + 18.0f;
    float barW = panelW - 54.0f;
    float barH = 7.0f;
    drawList->AddRectFilled(ImVec2(barX, barY), ImVec2(barX + barW, barY + barH), IM_COL32(25, 30, 40, 255), 2.0f);

    if (isReloading)
    {
        drawList->AddRectFilled(ImVec2(barX, barY), ImVec2(barX + barW * reloadProgress, barY + barH), IM_COL32(0, 255, 140, 255), 2.0f);
    }
    else
    {
        float ammoRatio = maxAmmo > 0 ? static_cast<float>(ammo) / maxAmmo : 0.0f;
        ImU32 barCol = IM_COL32(0, 200, 255, 240);
        if (ammo <= 6) barCol = IM_COL32(255, 160, 30, 240);
        if (ammo == 0) barCol = IM_COL32(255, 40, 40, 240);
        drawList->AddRectFilled(ImVec2(barX, barY), ImVec2(barX + barW * ammoRatio, barY + barH), barCol, 2.0f);
    }

    // 2. 救急キット (MEDKIT) アイコン ＆ 治療進行
    int medkits = ctx.player->GetMedkitCount();
    bool isHealing = ctx.player->IsHealing();
    float healProgress = ctx.player->GetHealProgress();
    float row2Y = panelY + 44.0f;

    if (medkitTextureIndex_ != UINT32_MAX)
    {
        uint64_t medGpu = TextureManager::GetInstance()->GetSrvHandleGPU(medkitTextureIndex_).ptr;
        if (medGpu != 0)
        {
            drawList->AddImageRounded((ImTextureID)medGpu, ImVec2(panelX + 10.0f, row2Y), ImVec2(panelX + 10.0f + iconSize, row2Y + iconSize), ImVec2(0, 0), ImVec2(1, 1), IM_COL32(255, 255, 255, 255), 4.0f);
            drawList->AddRect(ImVec2(panelX + 10.0f, row2Y), ImVec2(panelX + 10.0f + iconSize, row2Y + iconSize), IM_COL32(0, 230, 140, 180), 4.0f, 0, 1.0f);
        }
    }

    char medText[64];
    if (isHealing)
    {
        sprintf_s(medText, "🩹 HEALING... %d%%", static_cast<int>(healProgress * 100.0f));
        drawList->AddText(ImVec2(textOffsetX, row2Y), IM_COL32(0, 255, 180, 255), medText);

        float hBarY = row2Y + 18.0f;
        drawList->AddRectFilled(ImVec2(barX, hBarY), ImVec2(barX + barW, hBarY + 7.0f), IM_COL32(20, 35, 25, 255), 2.0f);
        drawList->AddRectFilled(ImVec2(barX, hBarY), ImVec2(barX + barW * healProgress, hBarY + 7.0f), IM_COL32(0, 255, 160, 255), 2.0f);
    }
    else
    {
        sprintf_s(medText, "[Q] MEDKIT: x%d (HP+40)", medkits);
        ImU32 medCol = (medkits > 0) ? IM_COL32(0, 230, 140, 255) : IM_COL32(140, 150, 160, 180);
        drawList->AddText(ImVec2(textOffsetX, row2Y + 4.0f), medCol, medText);
    }

    // 3. 金の鴨像 / 持ち帰り物資総額 アイコン
    int lootVal = ctx.player->GetLootValue();
    float row3Y = panelY + 80.0f;
    if (goldDuckTextureIndex_ != UINT32_MAX)
    {
        uint64_t goldGpu = TextureManager::GetInstance()->GetSrvHandleGPU(goldDuckTextureIndex_).ptr;
        if (goldGpu != 0)
        {
            drawList->AddImageRounded((ImTextureID)goldGpu, ImVec2(panelX + 10.0f, row3Y), ImVec2(panelX + 10.0f + iconSize, row3Y + iconSize), ImVec2(0, 0), ImVec2(1, 1), IM_COL32(255, 255, 255, 255), 4.0f);
            drawList->AddRect(ImVec2(panelX + 10.0f, row3Y), ImVec2(panelX + 10.0f + iconSize, row3Y + iconSize), IM_COL32(255, 215, 60, 200), 4.0f, 0, 1.0f);
        }
    }

    char lootText[64];
    sprintf_s(lootText, "💰 LOOT VALUE: $%d", lootVal);
    drawList->AddText(ImVec2(textOffsetX, row3Y + 5.0f), IM_COL32(255, 215, 60, 255), lootText);
#endif
}

// =============================================================================
// 3D物資探索カード HUD (フィールド上)
// =============================================================================

void GamePlayHUD::DrawLootingHUD(const GamePlayHUDContext& ctx)
{
#ifdef USE_IMGUI
    if (!ctx.player || ctx.player->IsDead() || !ctx.camera || ctx.isDeathSequenceActive || !ctx.lootProps) return;

    ImGuiIO& io = ImGui::GetIO();
    ImDrawList* drawList = ImGui::GetForegroundDrawList();
    float screenW = io.DisplaySize.x;
    float screenH = io.DisplaySize.y;

    const Matrix4x4& vp = ctx.camera->GetViewProjectionMatrix();
    const Vector3 playerPos = ctx.player->GetPosition();

    for (const auto& prop : *ctx.lootProps)
    {
        if (prop.isLooted) continue;

        float dx = prop.position.x - playerPos.x;
        float dz = prop.position.z - playerPos.z;
        float dist = std::sqrt(dx * dx + dz * dz);

        if (dist <= 7.0f)
        {
            Vector2 badge2D;
            Vector3 bPos = prop.position + Vector3{ 0.0f, 0.9f, 0.0f };
            if (Project3DTo2D(bPos, vp, screenW, screenH, badge2D))
            {
                bool isNearby = (dist <= LootSystem::kInteractRadius);
                float searchRatio = prop.maxSearchTime > 0.0f ? prop.searchTimer / prop.maxSearchTime : 0.0f;

                uint32_t iconTex = UINT32_MAX;
                if (prop.type == LootType::Medkit) iconTex = medkitTextureIndex_;
                else if (prop.type == LootType::AmmoBox) iconTex = ammoTextureIndex_;
                else if (prop.type == LootType::GoldDuck) iconTex = goldDuckTextureIndex_;
                else if (prop.type == LootType::Roubles) iconTex = roublesTextureIndex_;

                if (isNearby)
                {
                    float cardW = 220.0f;
                    float cardH = (prop.searchTimer > 0.0f) ? 56.0f : 46.0f;
                    float cx = badge2D.x - cardW * 0.5f;
                    float cy = badge2D.y - cardH * 0.5f;

                    drawList->AddRectFilled(ImVec2(cx, cy), ImVec2(cx + cardW, cy + cardH), IM_COL32(14, 22, 28, 245), 6.0f);
                    ImU32 cBorder = (prop.type == LootType::GoldDuck) ? IM_COL32(255, 215, 60, 255) : IM_COL32(0, 255, 140, 230);
                    drawList->AddRect(ImVec2(cx, cy), ImVec2(cx + cardW, cy + cardH), cBorder, 6.0f, 0, 1.5f);

                    if (iconTex != UINT32_MAX)
                    {
                        uint64_t iGpu = TextureManager::GetInstance()->GetSrvHandleGPU(iconTex).ptr;
                        if (iGpu != 0)
                        {
                            drawList->AddImageRounded((ImTextureID)iGpu, ImVec2(cx + 6.0f, cy + 6.0f), ImVec2(cx + 40.0f, cy + 40.0f), ImVec2(0, 0), ImVec2(1, 1), IM_COL32(255, 255, 255, 255), 4.0f);
                            drawList->AddRect(ImVec2(cx + 6.0f, cy + 6.0f), ImVec2(cx + 40.0f, cy + 40.0f), cBorder, 4.0f, 0, 1.0f);
                        }
                    }

                    float textX = cx + 46.0f;
                    if (prop.searchTimer > 0.0f)
                    {
                        char sBuf[64];
                        sprintf_s(sBuf, "SEARCHING... %d%%", static_cast<int>(searchRatio * 100.0f));
                        drawList->AddText(ImVec2(textX, cy + 6.0f), IM_COL32(255, 220, 60, 255), sBuf);

                        float pBarX = textX;
                        float pBarY = cy + 28.0f;
                        float pBarW = cardW - 54.0f;
                        drawList->AddRectFilled(ImVec2(pBarX, pBarY), ImVec2(pBarX + pBarW, pBarY + 8.0f), IM_COL32(25, 35, 30, 255), 2.0f);
                        drawList->AddRectFilled(ImVec2(pBarX, pBarY), ImVec2(pBarX + pBarW * searchRatio, pBarY + 8.0f), IM_COL32(0, 255, 140, 255), 2.0f);
                    }
                    else
                    {
                        drawList->AddText(ImVec2(textX, cy + 6.0f), IM_COL32(240, 245, 250, 255), prop.name.c_str());
                        drawList->AddText(ImVec2(textX, cy + 24.0f), IM_COL32(0, 255, 140, 255), "[E] HOLD TO LOOT");
                    }
                }
                else
                {
                    char bBuf[128];
                    sprintf_s(bBuf, "📦 %s [%.1fm]", prop.name.c_str(), dist);
                    ImVec2 bSize = ImGui::CalcTextSize(bBuf);
                    float bx = badge2D.x - bSize.x * 0.5f - 8.0f;
                    float by = badge2D.y - 12.0f;
                    float bw = bSize.x + 16.0f;
                    float bh = 24.0f;

                    drawList->AddRectFilled(ImVec2(bx, by), ImVec2(bx + bw, by + bh), IM_COL32(15, 20, 25, 180), 4.0f);
                    drawList->AddRect(ImVec2(bx, by), ImVec2(bx + bw, by + bh), IM_COL32(100, 140, 160, 160), 4.0f, 0, 1.5f);
                    drawList->AddText(ImVec2(badge2D.x - bSize.x * 0.5f, by + 4.0f), IM_COL32(200, 215, 225, 220), bBuf);
                }
            }
        }
    }
#endif
}

// =============================================================================
// 被弾インジケーター & 低体力ビネット & 死亡演出 HUD
// =============================================================================

void GamePlayHUD::DrawDamageIndicator(const GamePlayHUDContext& ctx)
{
#ifdef USE_IMGUI
    if (ctx.hitIndicatorTimer <= 0.0f || !ctx.player || ctx.isDeathSequenceActive) return;

    ImGuiIO& io = ImGui::GetIO();
    ImDrawList* drawList = ImGui::GetForegroundDrawList();
    float screenW = io.DisplaySize.x;
    float screenH = io.DisplaySize.y;
    float cx = screenW * 0.5f;
    float cy = screenH * 0.5f;

    float playerYaw = ctx.player->GetRotation().y;
    float relAngle = ctx.hitIndicatorAngle - playerYaw;

    float arcRadius = (std::min)(screenW, screenH) * 0.22f;
    float startRad = relAngle - 0.45f - 1.5707963f;
    float endRad   = relAngle + 0.45f - 1.5707963f;

    float alpha = ctx.hitIndicatorTimer / 0.65f;
    ImU32 arcCol = IM_COL32(255, 30, 30, static_cast<int>(alpha * 240.0f));

    drawList->PathArcTo(ImVec2(cx, cy), arcRadius, startRad, endRad, 24);
    drawList->PathStroke(arcCol, 0, 6.0f);
#endif
}

void GamePlayHUD::DrawLowHpRedVignetteEffect(const GamePlayHUDContext& ctx)
{
#ifdef USE_IMGUI
    if (!ctx.player || ctx.isDeathSequenceActive) return;

    float hpRatio = ctx.player->GetHPRatio();
    if (hpRatio >= 0.40f) return;

    ImGuiIO& io = ImGui::GetIO();
    ImDrawList* drawList = ImGui::GetForegroundDrawList();
    float screenW = io.DisplaySize.x;
    float screenH = io.DisplaySize.y;

    float dangerFactor = (0.40f - hpRatio) / 0.40f;
    float pulse = 0.75f + 0.25f * std::sin(static_cast<float>(ImGui::GetTime()) * (4.0f + dangerFactor * 6.0f));
    int edgeAlpha = static_cast<int>(180.0f * dangerFactor * pulse);

    float edgeThickness = (std::min)(screenW, screenH) * 0.18f;
    drawList->AddRectFilledMultiColor(ImVec2(0, 0), ImVec2(screenW, edgeThickness), IM_COL32(180, 10, 10, edgeAlpha), IM_COL32(180, 10, 10, edgeAlpha), IM_COL32(180, 10, 10, 0), IM_COL32(180, 10, 10, 0));
    drawList->AddRectFilledMultiColor(ImVec2(0, screenH - edgeThickness), ImVec2(screenW, screenH), IM_COL32(180, 10, 10, 0), IM_COL32(180, 10, 10, 0), IM_COL32(180, 10, 10, edgeAlpha), IM_COL32(180, 10, 10, edgeAlpha));
#endif
}

void GamePlayHUD::DrawDeathSequenceHUD(const GamePlayHUDContext& ctx)
{
#ifdef USE_IMGUI
    if (!ctx.isDeathSequenceActive) return;

    ImGuiIO& io = ImGui::GetIO();
    ImDrawList* drawList = ImGui::GetForegroundDrawList();
    float screenW = io.DisplaySize.x;
    float screenH = io.DisplaySize.y;

    float progress = ctx.deathSequenceTimer / 2.0f;
    float fadeAlpha = (std::min)(1.0f, progress * 1.5f);

    drawList->AddRectFilled(ImVec2(0, 0), ImVec2(screenW, screenH), IM_COL32(10, 5, 5, static_cast<int>(fadeAlpha * 225.0f)));

    float bannerH = 120.0f;
    float bannerY = (screenH - bannerH) * 0.45f;
    drawList->AddRectFilled(ImVec2(0, bannerY), ImVec2(screenW, bannerY + bannerH), IM_COL32(35, 10, 12, static_cast<int>(fadeAlpha * 240.0f)));
    drawList->AddLine(ImVec2(0, bannerY), ImVec2(screenW, bannerY), IM_COL32(255, 40, 40, static_cast<int>(fadeAlpha * 255.0f)), 2.0f);
    drawList->AddLine(ImVec2(0, bannerY + bannerH), ImVec2(screenW, bannerY + bannerH), IM_COL32(255, 40, 40, static_cast<int>(fadeAlpha * 255.0f)), 2.0f);

    const char* headText = "💀 KILLED IN ACTION // 戦闘不能";
    ImVec2 headSz = ImGui::CalcTextSize(headText);
    drawList->AddText(ImVec2((screenW - headSz.x) * 0.5f, bannerY + 22.0f), IM_COL32(255, 60, 60, static_cast<int>(fadeAlpha * 255.0f)), headText);

    char detailBuf[256];
    std::string killer = ctx.player ? ctx.player->GetLastAttackerName() : "HOSTILE FORCES";
    std::string cause = ctx.player ? ctx.player->GetCauseOfDeath() : "GUNFIRE DAMAGE";
    sprintf_s(detailBuf, "KILLER: %s  |  CAUSE: %s", killer.c_str(), cause.c_str());
    ImVec2 detSz = ImGui::CalcTextSize(detailBuf);
    drawList->AddText(ImVec2((screenW - detSz.x) * 0.5f, bannerY + 68.0f), IM_COL32(255, 200, 200, static_cast<int>(fadeAlpha * 240.0f)), detailBuf);
#endif
}

// =============================================================================
// レーザーサイト ＆ 浮遊テキスト ＆ デバッグギズモ HUD
// =============================================================================

void GamePlayHUD::DrawLaserSight(const GamePlayHUDContext& ctx)
{
#ifdef USE_IMGUI
    if (!ctx.player || ctx.player->IsDead() || !ctx.camera || ctx.isDeathSequenceActive) return;

    ImGuiIO& io = ImGui::GetIO();
    ImDrawList* drawList = ImGui::GetForegroundDrawList();
    float screenW = io.DisplaySize.x;
    float screenH = io.DisplaySize.y;

    const Matrix4x4& vp = ctx.camera->GetViewProjectionMatrix();
    const float yaw = ctx.player->GetRotation().y;
    const Vector3 forward = { std::sin(yaw), 0.0f, std::cos(yaw) };
    const Vector3 muzzlePos = ctx.player->GetPosition() + Vector3{ 0.0f, 0.70f, 0.0f } + forward * 0.95f;

    float maxLaserDist = 28.0f;
    float closestDist = maxLaserDist;

    // 障害物コライダーとのレイキャスト判定
    if (ctx.obstacles)
    {
        for (const auto& obs : *ctx.obstacles)
        {
            if (!obs) continue;
            auto* box = obs->GetCollider();
            if (box)
            {
                Vector3 center = box->GetWorldPosition();
                Vector3 extents = box->GetExtents();
                Vector3 bMin = center - extents;
                Vector3 bMax = center + extents;
                float tMin = 0.0f;
                float tMax = maxLaserDist;

                auto checkAxis = [&](float o, float d, float minV, float maxV) -> bool {
                    if (std::abs(d) < 1e-6f) return o >= minV && o <= maxV;
                    float t1 = (minV - o) / d;
                    float t2 = (maxV - o) / d;
                    if (t1 > t2) std::swap(t1, t2);
                    tMin = (std::max)(tMin, t1);
                    tMax = (std::min)(tMax, t2);
                    return tMin <= tMax;
                };

                if (checkAxis(muzzlePos.x, forward.x, bMin.x, bMax.x) &&
                    checkAxis(muzzlePos.y, forward.y, bMin.y, bMax.y) &&
                    checkAxis(muzzlePos.z, forward.z, bMin.z, bMax.z))
                {
                    if (tMin > 0.0f && tMin < closestDist) closestDist = tMin;
                }
            }
        }
    }

    Vector3 laserEnd = muzzlePos + forward * closestDist;
    Vector2 m2D, e2D;
    if (Project3DTo2D(muzzlePos, vp, screenW, screenH, m2D) && Project3DTo2D(laserEnd, vp, screenW, screenH, e2D))
    {
        drawList->AddLine(ImVec2(m2D.x, m2D.y), ImVec2(e2D.x, e2D.y), IM_COL32(255, 30, 30, 160), 1.5f);
        drawList->AddCircleFilled(ImVec2(e2D.x, e2D.y), 3.5f, IM_COL32(255, 60, 60, 240));
    }
#endif
}

void GamePlayHUD::DrawFloatingTexts(const GamePlayHUDContext& ctx)
{
#ifdef USE_IMGUI
    if (!ctx.camera || !ctx.floatingTexts) return;

    ImGuiIO& io = ImGui::GetIO();
    ImDrawList* drawList = ImGui::GetForegroundDrawList();
    float screenW = io.DisplaySize.x;
    float screenH = io.DisplaySize.y;

    const Matrix4x4& vp = ctx.camera->GetViewProjectionMatrix();

    for (const auto& ft : *ctx.floatingTexts)
    {
        if (ft.lifeTime <= 0.0f) continue;

        Vector2 p2D;
        if (Project3DTo2D(ft.position, vp, screenW, screenH, p2D))
        {
            float alpha = (ft.maxLifeTime > 0.0f) ? ft.lifeTime / ft.maxLifeTime : 1.0f;
            ImU32 col = IM_COL32(static_cast<int>(ft.color.x * 255), static_cast<int>(ft.color.y * 255), static_cast<int>(ft.color.z * 255), static_cast<int>(alpha * 255));

            ImVec2 sz = ImGui::CalcTextSize(ft.text.c_str());
            drawList->AddText(ImVec2(p2D.x - sz.x * 0.5f, p2D.y - sz.y * 0.5f), col, ft.text.c_str());
        }
    }
#endif
}

void GamePlayHUD::DrawVisionConesAndGizmos(const GamePlayHUDContext& ctx)
{
#ifdef USE_IMGUI
    if (!ctx.showDebugGizmos || !ctx.camera) return;

    ImDrawList* drawList = ImGui::GetForegroundDrawList();
    ImGuiIO& io = ImGui::GetIO();
    float screenW = io.DisplaySize.x;
    float screenH = io.DisplaySize.y;
    const Matrix4x4& vp = ctx.camera->GetViewProjectionMatrix();

    // プレイヤーの足音ノイズリング描画
    if (ctx.player && ctx.playerSoundTimer > 0.0f)
    {
        float ratio = ctx.playerSoundTimer / 0.5f;
        float curR = ctx.playerSoundMaxRadius * (1.0f - ratio);
        int segs = 32;
        Vector3 pCenter = ctx.player->GetPosition();

        for (int i = 0; i < segs; ++i)
        {
            float a1 = (i / (float)segs) * 6.2831853f;
            float a2 = ((i + 1) / (float)segs) * 6.2831853f;
            Vector3 p1 = pCenter + Vector3{ std::cos(a1) * curR, 0.05f, std::sin(a1) * curR };
            Vector3 p2 = pCenter + Vector3{ std::cos(a2) * curR, 0.05f, std::sin(a2) * curR };
            Vector2 s1, s2;
            if (Project3DTo2D(p1, vp, screenW, screenH, s1) && Project3DTo2D(p2, vp, screenW, screenH, s2))
            {
                drawList->AddLine(ImVec2(s1.x, s1.y), ImVec2(s2.x, s2.y), IM_COL32(0, 200, 255, static_cast<int>(ratio * 200)), 2.0f);
            }
        }
    }
#endif
}

void GamePlayHUD::DrawPerformanceTrackerUI(const GamePlayHUDContext& ctx, float deltaTime)
{
#ifdef USE_IMGUI
    if (!ctx.showPerformanceTracker) return;

    ImGui::SetNextWindowPos(ImVec2(16.0f, 60.0f), ImGuiCond_FirstUseEver);
    ImGui::SetNextWindowSize(ImVec2(290.0f, 210.0f), ImGuiCond_FirstUseEver);

    if (ImGui::Begin("Duckov Profiler & Engine Monitor", nullptr, ImGuiWindowFlags_AlwaysAutoResize))
    {
        float fps = (deltaTime > 0.0f) ? 1.0f / deltaTime : 0.0f;
        ImGui::Text("FPS: %.1f (FrameTime: %.2f ms)", fps, deltaTime * 1000.0f);
        ImGui::Separator();

        if (ctx.player)
        {
            ImGui::Text("HP: %.1f / %.1f", ctx.player->GetHP(), ctx.player->GetMaxHP());
            ImGui::Text("Ammo: %d / %d (Res: %d)", ctx.player->GetMagazineAmmo(), ctx.player->GetMaxMagazineAmmo(), ctx.player->GetReserveAmmo());
            ImGui::Text("Medkits: %d", ctx.player->GetMedkitCount());
            ImGui::Text("Loot Value: $%d", ctx.player->GetLootValue());
        }

        ImGui::Separator();
        ImGui::Text("Active Props: %zu", ctx.lootProps ? ctx.lootProps->size() : 0);
        ImGui::Text("Stress Test: %s (%d objs)", ctx.isStressTestActive ? "ACTIVE" : "OFF", ctx.stressTestCount);
    }
    ImGui::End();
#endif
}

void GamePlayHUD::DrawTutorialSignHUD(const GamePlayHUDContext& ctx)
{
#ifdef USE_IMGUI
    if (!ctx.tutorialSigns || !ctx.camera) return;

    ImGuiIO& io = ImGui::GetIO();
    float screenW = io.DisplaySize.x;
    float screenH = io.DisplaySize.y;
    if (screenW <= 0.0f || screenH <= 0.0f) return;

    const Matrix4x4& vp = ctx.camera->GetViewProjectionMatrix();

    for (const auto& sign : *ctx.tutorialSigns)
    {
        if (!sign || !sign->IsPlayerNear()) continue;

        Vector3 signPos = sign->GetPosition();
        signPos.y += 1.8f; // 看板の頭上

        Vector2 screenPos;
        bool onScreen = Project3DTo2D(signPos, vp, screenW, screenH, screenPos);

        // 画面外の場合は画面中央下部に固定表示
        if (!onScreen || screenPos.x < 50.0f || screenPos.x > screenW - 50.0f || screenPos.y < 50.0f || screenPos.y > screenH - 50.0f)
        {
            screenPos = { screenW * 0.5f, screenH * 0.72f };
        }

        std::string msg = sign->GetMessage();

        ImGui::SetNextWindowPos(ImVec2(screenPos.x, screenPos.y), ImGuiCond_Always, ImVec2(0.5f, 1.0f));
        ImGui::SetNextWindowBgAlpha(0.88f);

        ImGuiWindowFlags flags = ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_AlwaysAutoResize |
                                 ImGuiWindowFlags_NoSavedSettings | ImGuiWindowFlags_NoFocusOnAppearing |
                                 ImGuiWindowFlags_NoNav | ImGuiWindowFlags_NoMove;

        std::string winId = "TutorialSign##" + std::to_string(reinterpret_cast<uintptr_t>(sign.get()));
        if (ImGui::Begin(winId.c_str(), nullptr, flags))
        {
            ImGui::TextColored(ImVec4(1.0f, 0.85f, 0.2f, 1.0f), (const char*)u8"📜 [ TUTORIAL GUIDE - 作戦教本 ]");
            ImGui::Separator();
            ImGui::Spacing();
            ImGui::TextColored(ImVec4(1.0f, 1.0f, 1.0f, 1.0f), "%s", msg.c_str());
            ImGui::Spacing();
        }
        ImGui::End();
    }
#endif
}

