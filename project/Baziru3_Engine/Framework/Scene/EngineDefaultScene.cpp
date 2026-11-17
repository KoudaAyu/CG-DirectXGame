#include "EngineDefaultScene.h"
#include "BioProceduralGenerator.h"
#include "SceneManager.h"
#include "SceneRenderRequests.h"
#include "TextureManager.h"
#include "WindowsAPI.h"

#include <Windows.h>
#include <algorithm>
#include <cmath>
#include <iostream>
#include <random>

#ifdef USE_IMGUI
#include <imgui.h>
#endif

namespace
{
constexpr float kPi = 3.14159265358979323846f;
constexpr float kDegToRad = kPi / 180.0f;

bool IsWindowActiveAndFocused(DirectXCom* dx)
{
    if (!dx || !dx->GetWindowAPI())
    {
        return false;
    }
    return GetForegroundWindow() == dx->GetWindowAPI()->GetHwnd();
}
} // namespace

void EngineDefaultScene::InitializeScene()
{
    timer_ = 0.0f;
    if (!dxCommon_)
    {
        return;
    }

    using namespace BaziruEngine::Environment;

    // オフスクリーンレンダリングのデフォルトをリアル描画向け (Normal) に設定
    if (sceneManager_ && sceneManager_->GetOffScreenRendering())
    {
        sceneManager_->GetOffScreenRendering()->SetPostEffect(OffScreenRendering::PostEffect::Normal);
    }

    windSystem_ = std::make_unique<WindSystem>();
    windSystem_->Initialize(dxCommon_);

    natureRenderer_ = std::make_unique<NatureMeshRenderer>();
    natureRenderer_->Initialize(dxCommon_, std::cout);

    foliageField_ = std::make_unique<FoliageField>();
    foliageField_->Initialize(dxCommon_, std::cout, 36000);

    waterSurface_ = std::make_unique<WaterSurface>();
    waterSurface_->InitializeGrid(dxCommon_, std::cout, 72.0f, 72.0f, 168, 168, waterLevel_);

    // 地形メッシュ生成 (72m x 72m)
    std::vector<NatureVertex> terrainVerts;
    std::vector<uint32_t> terrainIndices;
    NatureMeshRenderer::BuildRiversideTerrainMesh(72.0f, 72.0f, 168, 168, waterLevel_, terrainVerts, terrainIndices);
    terrainMesh_ = natureRenderer_->UploadMesh(dxCommon_, terrainVerts, terrainIndices);

    // 押し分けオーブ可視化用の小石マーカーメッシュ
    BioProcedural::RockParameters orbParams{};
    orbParams.scale = 0.35f;
    orbParams.subdivisions = 3;
    orbParams.noiseStrength = 0.06f;
    orbParams.voronoiStrength = 0.04f;
    orbParams.crackStrength = 0.0f;
    orbMarkerMesh_ = natureRenderer_->CreateRockMeshFromProcedural(
        dxCommon_, BioProcedural::BioProceduralGenerator::GenerateRock(orbParams), { 0.95f, 0.88f, 0.55f });

    // 岩石・樹木・28,000本の3D草を一括生成
    RebuildProceduralAssets(currentSeed_);

    // 初期カメラアングル（川と揺れる草原・木々・水中岩が一望できる美しい構図）
    ApplyCameraPreset(0);
    UpdateSunAndAtmosphere();

    initializedLab_ = true;
}

void EngineDefaultScene::Finalize()
{
    if (waterSurface_)
    {
        waterSurface_->Finalize();
        waterSurface_.reset();
    }
    if (foliageField_)
    {
        foliageField_->Finalize();
        foliageField_.reset();
    }
    if (natureRenderer_)
    {
        natureRenderer_->Finalize();
        natureRenderer_.reset();
    }
    if (windSystem_)
    {
        windSystem_->Finalize();
        windSystem_.reset();
    }

    terrainMesh_ = {};
    for (auto& rm : rockMeshes_) rm = {};
    for (auto& tm : treeMeshes_) tm = {};
    orbMarkerMesh_ = {};
    placedRocks_.clear();
    placedTrees_.clear();
    initializedLab_ = false;
}

void EngineDefaultScene::RebuildProceduralAssets(uint32_t seed)
{
    if (!dxCommon_ || !natureRenderer_ || !foliageField_)
    {
        return;
    }

    using namespace BaziruEngine::Environment;

    // 1. 4種類のプロシージャルボロノイ岩石メッシュ生成
    const Vector3 rockPalettes[4] = {
        { 0.45f, 0.44f, 0.41f },
        { 0.39f, 0.41f, 0.43f },
        { 0.48f, 0.45f, 0.39f },
        { 0.36f, 0.37f, 0.36f }
    };
    for (uint32_t i = 0; i < 4; ++i)
    {
        BioProcedural::RockParameters rp{};
        rp.scale = 1.0f;
        rp.subdivisions = 4;
        rp.noiseStrength = 0.28f;
        rp.voronoiStrength = 0.22f;
        rp.crackStrength = 0.32f;
        rp.seed = seed + i * 7919u;
        rockMeshes_[i] = natureRenderer_->CreateRockMeshFromProcedural(
            dxCommon_, BioProcedural::BioProceduralGenerator::GenerateRock(rp), rockPalettes[i]);
    }

    // 2. 3種類のリアル広葉樹メッシュ生成（うち1種は黄金色のアクセント樹）
    for (uint32_t i = 0; i < 3; ++i)
    {
        std::vector<NatureVertex> tv;
        std::vector<uint32_t> ti;
        bool isAutumn = (i == 2);
        NatureMeshRenderer::BuildRealisticTreeMesh(seed + 1000u + i * 3571u, 1.0f + 0.12f * static_cast<float>(i), isAutumn, tv, ti);
        treeMeshes_[i] = natureRenderer_->UploadMesh(dxCommon_, tv, ti);
    }

    // 3. 水中岩（泡＆コースティクス確認用）と岸辺の苔岩を配置
    placedRocks_.clear();
    std::mt19937 rng(seed + 42u);
    std::uniform_real_distribution<float> u01(0.0f, 1.0f);

    // 3a. 川の中に半水没する巨石・飛び石 (8個)
    const float riverRockZ[8] = { -18.0f, -11.5f, -6.2f, -1.8f, 3.4f, 8.8f, 14.5f, 20.0f };
    for (int i = 0; i < 8; ++i)
    {
        float rz = riverRockZ[i];
        float riverCx = NatureMeshRenderer::EvaluateRiverCenterX(rz);
        float offsetFromCenter = ((i % 2 == 0) ? -1.0f : 1.0f) * (1.2f + u01(rng) * 2.6f);
        float rx = riverCx + offsetFromCenter;
        float bedY = NatureMeshRenderer::EvaluateRiversideHeight(rx, rz);

        float sx = 1.05f + u01(rng) * 0.95f;
        float sy = 0.72f + u01(rng) * 0.55f;
        float sz = 0.95f + u01(rng) * 0.85f;
        // 水面から少し頭を出す岩と、完全に水面下に沈んで屈折・コースティクスが見える岩を両方配置
        float placeY = (i % 3 == 1) ? (waterLevel_ - sy * 0.55f) : (std::max)(bedY + sy * 0.45f, waterLevel_ - sy * 0.18f);

        PlacedObject obj{};
        obj.meshVariant = static_cast<uint32_t>(i % 4);
        obj.worldMatrix = MakeAffineMatrix(
            { sx, sy, sz },
            { u01(rng) * 0.25f, u01(rng) * 6.28f, u01(rng) * 0.25f },
            { rx, placeY, rz });
        obj.mossScale = 0.85f;
        placedRocks_.push_back(obj);
    }

    // 3b. 岸辺・草原の苔むした岩 (10個)
    for (int i = 0; i < 10; ++i)
    {
        float rz = -24.0f + static_cast<float>(i) * 5.2f + (u01(rng) - 0.5f) * 2.0f;
        float riverCx = NatureMeshRenderer::EvaluateRiverCenterX(rz);
        float side = (i % 2 == 0) ? -1.0f : 1.0f;
        float rx = riverCx + side * (6.2f + u01(rng) * 10.0f);
        float groundY = NatureMeshRenderer::EvaluateRiversideHeight(rx, rz);

        float sx = 0.85f + u01(rng) * 1.15f;
        float sy = 0.65f + u01(rng) * 0.75f;
        float sz = 0.85f + u01(rng) * 1.10f;

        PlacedObject obj{};
        obj.meshVariant = static_cast<uint32_t>((i + 1) % 4);
        obj.worldMatrix = MakeAffineMatrix(
            { sx, sy, sz },
            { (u01(rng) - 0.5f) * 0.2f, u01(rng) * 6.28f, (u01(rng) - 0.5f) * 0.2f },
            { rx, groundY + sy * 0.28f, rz });
        obj.mossScale = 1.25f;
        placedRocks_.push_back(obj);
    }

    // 4. 川沿いの木立を配置 (10本)
    placedTrees_.clear();
    const float treeZ[10] = { -22.0f, -15.5f, -9.0f, -3.5f, 2.5f, 7.5f, 13.0f, 18.5f, -12.0f, 10.5f };
    for (int i = 0; i < 10; ++i)
    {
        float tz = treeZ[i];
        float riverCx = NatureMeshRenderer::EvaluateRiverCenterX(tz);
        float side = (i % 2 == 0) ? 1.0f : -1.0f;
        float dist = (i >= 8) ? (15.5f + u01(rng) * 4.0f) : (7.8f + u01(rng) * 5.5f);
        float tx = riverCx + side * dist;
        float ty = NatureMeshRenderer::EvaluateRiversideHeight(tx, tz);

        float s = 0.92f + u01(rng) * 0.36f;
        PlacedObject tree{};
        tree.meshVariant = (i == 3 || i == 7) ? 2u : static_cast<uint32_t>(i % 2);
        tree.worldMatrix = MakeAffineMatrix(
            { s, s, s },
            { 0.0f, u01(rng) * 6.28f, 0.0f },
            { tx, ty - 0.08f, tz });
        placedTrees_.push_back(tree);
    }

    // 5. 28,000本のGPUインスタンシング3D草ブレードを地形に沿って配置
    uint32_t prevActive = foliageField_->GetActiveInstanceCount();
    uint32_t targetCount = (prevActive > 0) ? prevActive : 28000u;
    foliageField_->PopulateOnTerrain(
        64.0f, 64.0f, waterLevel_, 32000u, seed + 999u,
        &NatureMeshRenderer::EvaluateRiversideHeight,
        &NatureMeshRenderer::EvaluateRiversideNormal);
    foliageField_->SetActiveInstanceCount((std::min)(targetCount, foliageField_->GetTotalUploadedCount()));
}

void EngineDefaultScene::ApplyCameraPreset(int presetIndex)
{
    switch (presetIndex)
    {
    case 0: // 川辺と草原の全景
        camPos_ = { -8.8f, 3.15f, -15.8f };
        camPitch_ = 0.17f;
        camYaw_ = 0.56f;
        break;
    case 1: // 逆光のローアングル草原（SSS透過光＆風波確認用）
        camPos_ = { -9.5f, 1.38f, -5.2f };
        camPitch_ = 0.05f;
        camYaw_ = 0.68f;
        sunElevationDeg_ = 19.0f;
        sunAzimuthDeg_ = 218.0f;
        UpdateSunAndAtmosphere();
        break;
    case 2: // 水中の屈折・コースティクス・岩まわりの白泡クローズアップ
        camPos_ = { 0.4f, 1.85f, -8.8f };
        camPitch_ = 0.36f;
        camYaw_ = 0.22f;
        break;
    case 3: // ゴールデンアワー夕景
        camPos_ = { -10.2f, 2.75f, -13.5f };
        camPitch_ = 0.12f;
        camYaw_ = 0.58f;
        sunElevationDeg_ = 11.5f;
        sunAzimuthDeg_ = 212.0f;
        sunIntensity_ = 1.55f;
        UpdateSunAndAtmosphere();
        break;
    default:
        break;
    }

    if (camera_)
    {
        camera_->SetTranslate(camPos_);
        camera_->SetRotate({ camPitch_, camYaw_, 0.0f });
        camera_->Update();
    }
}

void EngineDefaultScene::UpdateSunAndAtmosphere()
{
    float elevRad = sunElevationDeg_ * kDegToRad;
    float azimRad = sunAzimuthDeg_ * kDegToRad;

    // 太陽が存在する方向ベクトル sunDir と、ライト進行方向 (-sunDir)
    Vector3 sunDir = {
        std::cos(elevRad) * std::sin(azimRad),
        std::sin(elevRad),
        std::cos(elevRad) * std::cos(azimRad)
    };
    Vector3 lightDirection = { -sunDir.x, -sunDir.y, -sunDir.z };

    // 太陽高度に応じて昼の爽やかな白太陽光〜夕方の黄金色太陽光＆大気フォグ色を自動補間
    float sunsetT = 1.0f - std::clamp((sunElevationDeg_ - 8.0f) / 32.0f, 0.0f, 1.0f);
    Vector4 sunColor = {
        1.0f,
        0.97f - 0.25f * sunsetT,
        0.90f - 0.48f * sunsetT,
        1.0f
    };
    currentFogColor_ = {
        0.60f + 0.32f * sunsetT,
        0.77f - 0.14f * sunsetT,
        0.94f - 0.42f * sunsetT
    };

    if (Light* light = GetLight())
    {
        light->SetDirectionalLight(sunColor, lightDirection, sunIntensity_);
    }
    if (foliageField_)
    {
        foliageField_->SetFog(currentFogColor_, fogDensity_);
    }
    if (waterSurface_)
    {
        waterSurface_->SetFog(currentFogColor_, fogDensity_);
    }
}

void EngineDefaultScene::UpdateCameraControl(float dt)
{
    if (!camera_)
    {
        return;
    }

    bool uiCapturingMouse = false;
    bool uiCapturingKey = false;
#ifdef USE_IMGUI
    if (ImGui::GetCurrentContext())
    {
        const ImGuiIO& io = ImGui::GetIO();
        uiCapturingMouse = io.WantCaptureMouse;
        uiCapturingKey = io.WantCaptureKeyboard;
    }
#endif

    const bool focused = IsWindowActiveAndFocused(dxCommon_);
    if (focused && !uiCapturingMouse && (GetAsyncKeyState(VK_RBUTTON) & 0x8000))
    {
        POINT pt{};
        if (GetCursorPos(&pt))
        {
            if (rmbDragging_)
            {
                float dx = static_cast<float>(pt.x - lastMousePt_.x);
                float dy = static_cast<float>(pt.y - lastMousePt_.y);
                camYaw_ += dx * 0.0032f;
                camPitch_ = std::clamp(camPitch_ + dy * 0.0032f, -1.35f, 1.35f);
            }
            lastMousePt_ = pt;
            rmbDragging_ = true;
        }
    }
    else
    {
        rmbDragging_ = false;
    }

    if (focused && !uiCapturingKey)
    {
        float speed = camMoveSpeed_ * ((GetAsyncKeyState(VK_SHIFT) & 0x8000) ? 2.4f : 1.0f);
        Vector3 forward = {
            std::cos(camPitch_) * std::sin(camYaw_),
            -std::sin(camPitch_),
            std::cos(camPitch_) * std::cos(camYaw_)
        };
        Vector3 right = { std::cos(camYaw_), 0.0f, -std::sin(camYaw_) };

        if (GetAsyncKeyState('W') & 0x8000) camPos_ = camPos_ + forward * (speed * dt);
        if (GetAsyncKeyState('S') & 0x8000) camPos_ = camPos_ - forward * (speed * dt);
        if (GetAsyncKeyState('D') & 0x8000) camPos_ = camPos_ + right * (speed * dt);
        if (GetAsyncKeyState('A') & 0x8000) camPos_ = camPos_ - right * (speed * dt);
        if (GetAsyncKeyState('E') & 0x8000) camPos_.y += speed * dt;
        if (GetAsyncKeyState('Q') & 0x8000) camPos_.y -= speed * dt;
    }

    camera_->SetTranslate(camPos_);
    camera_->SetRotate({ camPitch_, camYaw_, 0.0f });
    camera_->Update();
}

void EngineDefaultScene::Update()
{
    constexpr float kDt = 1.0f / 60.0f;
    timer_ += kDt;

    if (initializedLab_ && enableNatureLab_)
    {
        UpdateCameraControl(kDt);
        UpdateSunAndAtmosphere();

        // 草押し分けインタラクティブオーブの更新（自動周回または矢印キー手動操作）
        if (autoMoveRepulsor_)
        {
            float z = std::sin(timer_ * 0.55f) * 11.0f;
            float riverCx = BaziruEngine::Environment::NatureMeshRenderer::EvaluateRiverCenterX(z);
            float x = riverCx - 8.2f + std::cos(timer_ * 1.1f) * 2.2f;
            float y = BaziruEngine::Environment::NatureMeshRenderer::EvaluateRiversideHeight(x, z);
            repulsorPos_ = { x, y + 0.35f, z };
        }
        else if (IsWindowActiveAndFocused(dxCommon_))
        {
            const float moveStep = 6.5f * kDt;
            if (GetAsyncKeyState(VK_UP) & 0x8000)    repulsorPos_.z += moveStep;
            if (GetAsyncKeyState(VK_DOWN) & 0x8000)  repulsorPos_.z -= moveStep;
            if (GetAsyncKeyState(VK_RIGHT) & 0x8000) repulsorPos_.x += moveStep;
            if (GetAsyncKeyState(VK_LEFT) & 0x8000)  repulsorPos_.x -= moveStep;
            repulsorPos_.y = BaziruEngine::Environment::NatureMeshRenderer::EvaluateRiversideHeight(repulsorPos_.x, repulsorPos_.z) + 0.35f;
        }

        if (windSystem_)
        {
            windSystem_->SetRepulsor(0, repulsorPos_, repulsorRadius_);
            // カメラ自身が低い草原に入ったときも足元の草が自然に押し分けられる！
            windSystem_->SetRepulsor(1, camPos_, 1.35f);
            windSystem_->Update(kDt);
        }
        if (waterSurface_)
        {
            waterSurface_->SetWaterLevel(waterLevel_);
            waterSurface_->Update(kDt);
        }
    }

    DrawLabImGui();
}

void EngineDefaultScene::DrawLabImGui()
{
#ifdef USE_IMGUI
    if (!ImGui::GetCurrentContext())
    {
        return;
    }

    ImGui::SetNextWindowPos(ImVec2(830.0f, 14.0f), ImGuiCond_FirstUseEver);
    ImGui::SetNextWindowSize(ImVec2(435.0f, 680.0f), ImGuiCond_FirstUseEver);
    ImGui::Begin("Baziru3 Engine - Nature & Water Lab");

    ImGui::TextColored(ImVec4(0.35f, 0.92f, 0.65f, 1.0f), "Realistic Water & Foliage Sandbox");
    ImGui::TextDisabled("Camera: Right-Drag = Look | WASD/QE = Fly (Shift = Fast)");
    ImGui::Checkbox("Enable Nature Lab Showcase", &enableNatureLab_);

    if (sceneManager_)
    {
        auto scenes = sceneManager_->GetAvailableSceneNames();
        if (scenes.size() > 1)
        {
            ImGui::Separator();
            ImGui::Text("Switch Registered Scene:");
            for (const auto& name : scenes)
            {
                if (name == "DEFAULT") continue;
                if (ImGui::Button(name.c_str()))
                {
                    sceneManager_->ChangeScene(name);
                }
                ImGui::SameLine();
            }
            ImGui::NewLine();
        }
    }

    if (enableNatureLab_ && initializedLab_)
    {
        ImGui::Separator();
        ImGui::Text("Camera Presets:");
        if (ImGui::Button("1: Overview"))      ApplyCameraPreset(0);
        ImGui::SameLine();
        if (ImGui::Button("2: Backlit Grass")) ApplyCameraPreset(1);
        ImGui::SameLine();
        if (ImGui::Button("3: Water & Foam"))  ApplyCameraPreset(2);
        ImGui::SameLine();
        if (ImGui::Button("4: Sunset"))        ApplyCameraPreset(3);

        if (ImGui::CollapsingHeader("1. Water (Refraction / Depth / Caustics / Foam)", ImGuiTreeNodeFlags_DefaultOpen))
        {
            ImGui::SliderFloat("Water Level (m)", &waterLevel_, -0.8f, 0.8f, "%.2f");
            float waveH = waterSurface_->GetWaveHeight();
            if (ImGui::SliderFloat("Gerstner Wave Height", &waveH, 0.0f, 0.22f, "%.3f"))
                waterSurface_->SetWaveHeight(waveH);

            float flowSpd = waterSurface_->GetFlowSpeed();
            if (ImGui::SliderFloat("Flow Speed", &flowSpd, 0.0f, 3.0f, "%.2f"))
                waterSurface_->SetFlowSpeed(flowSpd);

            float refr = waterSurface_->GetRefractionScale();
            if (ImGui::SliderFloat("Refraction Distortion", &refr, 0.0f, 0.09f, "%.3f"))
                waterSurface_->SetRefractionScale(refr);

            float absorb = waterSurface_->GetAbsorptionDensity();
            if (ImGui::SliderFloat("Depth Absorption (Beer-Lambert)", &absorb, 0.2f, 3.0f, "%.2f"))
                waterSurface_->SetAbsorptionDensity(absorb);

            float caustics = waterSurface_->GetCausticsIntensity();
            if (ImGui::SliderFloat("Underwater Caustics", &caustics, 0.0f, 1.5f, "%.2f"))
                waterSurface_->SetCausticsIntensity(caustics);

            float foamW = waterSurface_->GetFoamWidth();
            if (ImGui::SliderFloat("Shore & Rock Foam Width", &foamW, 0.0f, 1.2f, "%.2f"))
                waterSurface_->SetFoamWidth(foamW);

            float foamI = waterSurface_->GetFoamIntensity();
            if (ImGui::SliderFloat("Foam Intensity", &foamI, 0.0f, 1.5f, "%.2f"))
                waterSurface_->SetFoamIntensity(foamI);
        }

        if (ImGui::CollapsingHeader("2. Grass & Foliage (GPU Instancing / Wind / SSS)", ImGuiTreeNodeFlags_DefaultOpen))
        {
            int activeGrass = static_cast<int>(foliageField_->GetActiveInstanceCount());
            int maxUploaded = static_cast<int>(foliageField_->GetTotalUploadedCount());
            if (ImGui::SliderInt("Active 3D Grass Blades", &activeGrass, 0, (std::max)(1, maxUploaded)))
            {
                foliageField_->SetActiveInstanceCount(static_cast<uint32_t>(activeGrass));
            }

            float windSpd = windSystem_->GetWindSpeed();
            if (ImGui::SliderFloat("Wind Speed", &windSpd, 0.0f, 6.0f, "%.2f"))
                windSystem_->SetWindSpeed(windSpd);

            float gustStr = windSystem_->GetGustStrength();
            if (ImGui::SliderFloat("Wind Gust Wave Strength", &gustStr, 0.0f, 1.8f, "%.2f"))
                windSystem_->SetGustStrength(gustStr);

            float turb = windSystem_->GetTurbulence();
            if (ImGui::SliderFloat("Leaf Flutter Turbulence", &turb, 0.0f, 1.2f, "%.2f"))
                windSystem_->SetTurbulence(turb);

            float grassSss = foliageField_->GetSssIntensity();
            if (ImGui::SliderFloat("Grass Backlit SSS", &grassSss, 0.0f, 2.5f, "%.2f"))
                foliageField_->SetSssIntensity(grassSss);

            ImGui::SliderFloat("Tree Leaf SSS", &treeSssIntensity_, 0.0f, 2.5f, "%.2f");
            ImGui::SliderFloat("Rock Velvet Moss", &mossIntensity_, 0.0f, 2.0f, "%.2f");

            ImGui::Checkbox("Auto-Orbit Grass Repulsor", &autoMoveRepulsor_);
            ImGui::SameLine();
            ImGui::Checkbox("Show Orb", &showRepulsorMarker_);
            if (!autoMoveRepulsor_)
            {
                ImGui::TextDisabled("Use Arrow Keys (Left/Right/Up/Down) to push grass!");
            }
            ImGui::SliderFloat("Repulsor Radius", &repulsorRadius_, 0.5f, 4.5f, "%.2f");
        }

        if (ImGui::CollapsingHeader("3. Sun, Time-of-Day & Procedural Seed", ImGuiTreeNodeFlags_DefaultOpen))
        {
            ImGui::SliderFloat("Sun Elevation (deg)", &sunElevationDeg_, 5.0f, 80.0f, "%.1f");
            ImGui::SliderFloat("Sun Azimuth (deg)", &sunAzimuthDeg_, 0.0f, 360.0f, "%.1f");
            ImGui::SliderFloat("Sun Intensity", &sunIntensity_, 0.3f, 2.5f, "%.2f");
            ImGui::SliderFloat("Atmospheric Fog", &fogDensity_, 0.0f, 0.015f, "%.4f");

            if (ImGui::Button("Regenerate Rocks, Trees & Grass (New Seed)"))
            {
                currentSeed_ = currentSeed_ * 1664525u + 1013904223u;
                RebuildProceduralAssets(currentSeed_);
            }
        }
    }

    ImGui::End();
#endif
}

void EngineDefaultScene::Draw(SceneRenderRequests& renderRequests)
{
    if (!enableNatureLab_ || !initializedLab_ || !dxCommon_ || !camera_ || !natureRenderer_)
    {
        return;
    }

    ID3D12GraphicsCommandList* cmdList = dxCommon_->GetCommandList().Get();
    if (!cmdList)
    {
        return;
    }

    Light* light = GetLight();
    OffScreenRendering* offScreen = sceneManager_ ? sceneManager_->GetOffScreenRendering() : nullptr;

    D3D12_GPU_DESCRIPTOR_HANDLE envSrv{};
    if (sceneManager_ && TextureManager::GetInstance())
    {
        uint32_t skyIdx = sceneManager_->GetSkyboxTextureIndex();
        if (skyIdx != TextureManager::kInvalidTextureIndex)
        {
            envSrv = TextureManager::GetInstance()->GetSrvHandleGPU(skyIdx);
        }
    }

    natureRenderer_->BeginFrame();

    // --- 1. 不透明・カットアウト自然物の描画（川底・地形・苔岩・広葉樹・3D草原） ---
    natureRenderer_->DrawMesh(
        cmdList, terrainMesh_, MakeIdentity4x4(), camera_, light, windSystem_.get(),
        envSrv, waterLevel_, mossIntensity_, 0.0f, currentFogColor_, fogDensity_);

    for (const auto& rock : placedRocks_)
    {
        natureRenderer_->DrawMesh(
            cmdList, rockMeshes_[rock.meshVariant], rock.worldMatrix, camera_, light, windSystem_.get(),
            envSrv, waterLevel_, mossIntensity_ * rock.mossScale, 0.0f, currentFogColor_, fogDensity_);
    }

    for (const auto& tree : placedTrees_)
    {
        natureRenderer_->DrawMesh(
            cmdList, treeMeshes_[tree.meshVariant], tree.worldMatrix, camera_, light, windSystem_.get(),
            envSrv, waterLevel_, mossIntensity_ * 0.4f, treeSssIntensity_, currentFogColor_, fogDensity_);
    }

    if (showRepulsorMarker_ && repulsorRadius_ > 0.05f)
    {
        Matrix4x4 orbWorld = MakeAffineMatrix(Vector3{ 1.0f, 1.0f, 1.0f }, Vector3{ 0.0f, timer_ * 1.5f, 0.0f }, repulsorPos_);
        natureRenderer_->DrawMesh(
            cmdList, orbMarkerMesh_, orbWorld, camera_, light, windSystem_.get(),
            envSrv, waterLevel_, 0.0f, 0.0f, currentFogColor_, fogDensity_, { 1.15f, 1.05f, 0.75f, 1.0f });
    }

    if (foliageField_)
    {
        foliageField_->Draw(cmdList, camera_, light, windSystem_.get());
    }

    // --- 2. シーンカラー＆深度スナップショットを取得し、屈折・水深減衰・泡付きのリアル水面を描画 ---
    if (waterSurface_ && offScreen)
    {
        waterSurface_->Draw(cmdList, offScreen, camera_, light, envSrv);
    }

    renderRequests.sceneDrawn = true;
}
