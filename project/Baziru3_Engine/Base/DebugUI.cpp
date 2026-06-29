#include "DebugUI.h"
#include "Matrix4x4.h"
#include "MaterialManager.h"
#include "SpriteManager.h"
#include "Camera.h"
#include "DebugCamera\DebugCamera.h"
#include "OffScreenRendering.h"
#include <imgui.h>
#include <string>
#include <fstream>
#include "SceneManager.h"
#include "GamePlayScene.h"
#include "ClearScene/ClearScene.h"

DebugUI::DebugUI(MaterialManager* materialManager, SpriteManager* spriteManager, Camera* camera,
    Sprite::Transform* transformObject, bool* useMonsterBall, bool* drawObject, bool* drawSprite,
    DebugCamera* debugCamera)
    : materialManager_(materialManager), spriteManager_(spriteManager), camera_(camera), transformObject_(transformObject), useMonsterBall_(useMonsterBall), drawObject_(drawObject), drawSprite_(drawSprite), debugCamera_(debugCamera)
{
}

void DebugUI::Initialize()
{
    // Try to load saved debug camera on init
    if (debugCamera_)
    {
        const std::string path = "debug_camera_save.bin";
        std::ifstream ifs(path, std::ios::binary);
        if (ifs)
        {
            debugCamera_->LoadFromFile(path);
        }
    }
}

void DebugUI::Update()
{
#ifdef USE_IMGUI
    ImGui::ShowDemoWindow();
    // Debug: indicate which DebugUI implementation is running (removed to avoid spam)

    ImGui::Begin("Windows");

    if (materialManager_)
    {
        ImGui::ColorEdit4("Material Color", &materialManager_->GetMaterialDataColor().x);
    }

    // Sprite position window: size (500,100), sliders (x,y) with initial (100,100) and format integer 4 digits, decimal 1
    ImGui::SetNextWindowSize(ImVec2(500.0f, 100.0f), ImGuiCond_Once);
    ImGui::Begin("Sprite Position");
    if (spriteManager_)
    {
        auto& sprites = spriteManager_->GetSprites();
        for (size_t i = 0; i < sprites.size(); ++i)
        {
            Sprite* s = sprites[i].get();
            if (!s) continue;

            // Get current position
            const Vector2& p = s->GetPosition();
            float posArr[2] = { p.x, p.y };
            char label[64];
            // Label each slider so ImGui state is unique per sprite
            std::snprintf(label, sizeof(label), "Sprite %zu Position", i);
            // Slider range chosen to cover typical window coordinates; format "%4.1f" meets the integer 4 digits and 1 decimal requirement
            if (ImGui::SliderFloat2(label, posArr, 0.0f, 2000.0f, "%4.1f"))
            {
                s->SetPosition({ posArr[0], posArr[1] });
            }
        }
    }
    ImGui::End();

    if (useMonsterBall_)
        ImGui::Checkbox("useMonsterBall", useMonsterBall_);

    if (materialManager_)
        ImGui::Checkbox("LightSprite Flag", (bool*)&materialManager_->GetMaterialDataEnableLighting());

    if (spriteManager_)
    {
        for (auto& spritePtr : spriteManager_->GetSprites())
        {
            ImGui::Checkbox("LightObject Flag", (bool*)&spritePtr->GetMaterialDataSprite()->enableLighting);
        }
    }

    if (drawObject_)
        ImGui::Checkbox("DrawObject", drawObject_);
    if (drawSprite_)
        ImGui::Checkbox("DrawSprite", drawSprite_);

    if (transformObject_)
        ImGui::DragFloat3("Object Rotate", &transformObject_->rotate.x, 0.01f, -10.0f, 10.0f);

    // Material collapsing header
    if (ImGui::CollapsingHeader("Material"))
    {
        if (materialManager_)
        {
            bool enableLock = (materialManager_->GetMaterialDataEnableLighting() != 0);
            if (ImGui::Checkbox("Enable Lighting", &enableLock))
            {
                materialManager_->GetMaterialDataEnableLighting() = enableLock ? 1 : 0;
            }

            ImGui::SliderFloat("Shininess", &materialManager_->GetMaterialDataShininess(), 0.1f, 100.0f);

            ImGui::SliderFloat("Reflection Factor", &materialManager_->GetMaterialReflectionFactor(), 0.0f, 1.0f);
            ImGui::SliderFloat("Fresnel F0", &materialManager_->GetMaterialFresnelF0(), 0.0f, 1.0f);

            ImGui::ColorEdit4("Material Color", &materialManager_->GetMaterialDataColor().x);
        }
    }

    if (ImGui::Button("Reset Camera") && camera_)
    {
        camera_->SetRotate({ 0.0f, 0.0f, 0.0f });
        camera_->SetTranslate({ 0.0f, 0.0f, -5.0f });
        camera_->Update();
    }

   
    if (camera_)
    {
        ImGui::Separator();
        if (ImGui::CollapsingHeader("Camera"))
        {
            // Position
            auto pos = camera_->GetTranslate();
            float posArr[3] = { pos.x, pos.y, pos.z };
            if (ImGui::DragFloat3("Camera Position", posArr, 0.1f))
            {
                camera_->SetTranslate({ posArr[0], posArr[1], posArr[2] });
                camera_->Update();
            }

            // Rotation
            auto rot = camera_->GetRotate();
            float rotArr[3] = { rot.x, rot.y, rot.z };
            if (ImGui::DragFloat3("Camera Rotate", rotArr, 0.01f))
            {
                camera_->SetRotate({ rotArr[0], rotArr[1], rotArr[2] });
                camera_->Update();
            }

            // FOV / near / far
            float fov = camera_->GetFovY();
            if (ImGui::SliderFloat("FOV (rad)", &fov, 0.1f, 1.5f))
            {
                camera_->SetFovY(fov);
                camera_->Update();
            }
            float nearZ = camera_->GetNearZ();
            if (ImGui::DragFloat("Near Z", &nearZ, 0.01f, 0.01f, 10.0f))
            {
                camera_->SetNearZ(nearZ);
                camera_->Update();
            }
            float farZ = camera_->GetFarZ();
            if (ImGui::DragFloat("Far Z", &farZ, 1.0f, 10.0f, 10000.0f))
            {
                camera_->SetFarZ(farZ);
                camera_->Update();
            }
        }
    }

    // DebugCamera editor: show as separate window so it's easy to find
    if (debugCamera_)
    {
        ImGui::Begin("Debug Camera");
        auto rot = debugCamera_->GetRotation();
        float rotArr[3] = { rot.x, rot.y, rot.z };
            if (ImGui::DragFloat3("DebugCam Rotate", rotArr, 0.01f))
            {
                // Always update debug camera rotation. Do not copy into main camera.
                debugCamera_->SetRotation({ rotArr[0], rotArr[1], rotArr[2] });
            }

        auto pos = debugCamera_->GetTranslation();
        float posArr[3] = { pos.x, pos.y, pos.z };
            if (ImGui::DragFloat3("DebugCam Position", posArr, 0.1f))
            {
                // Always update debug camera translation. Do not copy into main camera.
                debugCamera_->SetTranslation({ posArr[0], posArr[1], posArr[2] });
            }

        if (ImGui::Button("Save DebugCam"))
        {
            debugCamera_->SaveToFile("debug_camera_save.bin");
        }
        ImGui::SameLine();
        if (ImGui::Button("Load DebugCam"))
        {
            debugCamera_->LoadFromFile("debug_camera_save.bin");
        }
        ImGui::SameLine();
        if (ImGui::Button("Reset DebugCam"))
        {
            debugCamera_->Reset();
        }

        bool camMode = cameraMode_;
        if (ImGui::Checkbox("Use DebugCamera for Rendering", &camMode))
        {
            // camMode = true -> render using DebugCamera
            cameraMode_ = camMode;
            if (camera_ && debugCamera_)
            {
                // DebugCamera receives input when rendering with it; main Camera otherwise
                camera_->SetControlEnabled(!cameraMode_);
                debugCamera_->SetControlEnabled(cameraMode_);

                // Configure Camera to use DebugCamera view/projection when enabled
                camera_->SetDebugCameraOverride(debugCamera_);
                camera_->EnableDebugCameraOverride(cameraMode_);
            }
        }
        ImGui::End();
    }

    ImGui::End();

    ImGui::Begin("Material Settings");

    if (materialManager_)
    {
        bool enableLighting = (materialManager_->GetMaterialDataEnableLighting() != 0);
        if (ImGui::Checkbox("Enable Lighting", &enableLighting))
        {
            materialManager_->GetMaterialDataEnableLighting() = enableLighting ? 1 : 0;
        }

        ImGui::DragFloat("Shininess", &materialManager_->GetMaterialDataShininess(), 0.5f, 0.1f, 100.0f);

        ImGui::SliderFloat("Reflection Factor", &materialManager_->GetMaterialReflectionFactor(), 0.0f, 1.0f);
        ImGui::SliderFloat("Fresnel F0", &materialManager_->GetMaterialFresnelF0(), 0.0f, 1.0f);

        // Specular model selection
        int specModel = materialManager_->GetMaterialSpecularModel();
        const char* items = "Blinn-Phong\0Phong\0";
        if (ImGui::Combo("Specular Model", &specModel, items))
        {
            materialManager_->GetMaterialSpecularModel() = specModel;
        }
    }

    ImGui::End();

    ImGui::Begin("Settings");

    if (offScreenRendering_)
    {
        if (ImGui::CollapsingHeader("Post Effect"))
        {
            int currentEffect = static_cast<int>(offScreenRendering_->GetPostEffect());
            const char* effectNames[] = {
                "Normal",
                "DepthBasedOutline",
                "LuminanceBaseOutline",
                "RadialBlur",
                "GaussianFilter",
                "BoxFilter"
            };
            if (ImGui::Combo("Effect Type", &currentEffect, effectNames, IM_ARRAYSIZE(effectNames)))
            {
                offScreenRendering_->SetPostEffect(static_cast<OffScreenRendering::PostEffect>(currentEffect));
            }

            if (offScreenRendering_->GetPostEffect() == OffScreenRendering::PostEffect::RadialBlur)
            {
                Vector2 center = offScreenRendering_->GetRadialBlurCenter();
                float centerArr[2] = { center.x, center.y };
                if (ImGui::SliderFloat2("Blur Center", centerArr, 0.0f, 1.0f))
                {
                    offScreenRendering_->SetRadialBlurCenter({ centerArr[0], centerArr[1] });
                }

                float blurWidth = offScreenRendering_->GetRadialBlurWidth();
                if (ImGui::SliderFloat("Blur Width", &blurWidth, 0.0f, 0.1f, "%.4f"))
                {
                    offScreenRendering_->SetRadialBlurWidth(blurWidth);
                }
            }
        }
    }

    if (useMonsterBall_)
        ImGui::Checkbox("Use Monster Ball", useMonsterBall_);

    ImGui::Checkbox("Draw Skybox", SceneManager::GetInstance()->GetShowSkyboxPtr());

    ImGui::Separator();

    if (ImGui::CollapsingHeader("Material"))
    {
        if (materialManager_)
        {
            bool enable = (materialManager_->GetMaterialDataEnableLighting() != 0);
            if (ImGui::Checkbox("Enable Lighting", &enable))
            {
                materialManager_->GetMaterialDataEnableLighting() = enable ? 1 : 0;
            }

            ImGui::SliderFloat("Shininess", &materialManager_->GetMaterialDataShininess(), 0.1f, 100.0f);

            ImGui::SliderFloat("Reflection Factor", &materialManager_->GetMaterialReflectionFactor(), 0.0f, 1.0f);
            ImGui::SliderFloat("Fresnel F0", &materialManager_->GetMaterialFresnelF0(), 0.0f, 1.0f);

            ImGui::ColorEdit4("Color", &materialManager_->GetMaterialDataColor().x);
        }
    }

    ImGui::End();

	// ゲームプレイシーンの脱出状況を表示
	BaseScene* currentScene = SceneManager::GetInstance()->GetCurrentScene();
	if (currentScene && std::string(currentScene->GetSceneType()) == "GAMEPLAY")
	{
		GamePlayScene* gameplay = static_cast<GamePlayScene*>(currentScene);
		ImGuiIO& io = ImGui::GetIO();

		// 脱出ステータスウィンドウの設定
		ImGui::SetNextWindowPos(ImVec2(io.DisplaySize.x * 0.5f, io.DisplaySize.y * 0.25f), ImGuiCond_Always, ImVec2(0.5f, 0.5f));
		ImGui::Begin("Extraction Status", nullptr, 
			ImGuiWindowFlags_NoTitleBar | 
			ImGuiWindowFlags_NoResize | 
			ImGuiWindowFlags_NoMove | 
			ImGuiWindowFlags_NoScrollbar | 
			ImGuiWindowFlags_NoSavedSettings | 
			ImGuiWindowFlags_NoInputs | 
			ImGuiWindowFlags_AlwaysAutoResize | 
			ImGuiWindowFlags_NoBackground);

		float timer = gameplay->GetExtractionTimer();
		Player* player = gameplay->GetPlayer();
		if (player)
		{
			ImGui::SetNextWindowPos(ImVec2(20.0f, io.DisplaySize.y - 140.0f), ImGuiCond_Always);
			ImGui::Begin("Player Status HUD", nullptr,
				ImGuiWindowFlags_NoTitleBar |
				ImGuiWindowFlags_NoResize |
				ImGuiWindowFlags_NoMove |
				ImGuiWindowFlags_NoScrollbar |
				ImGuiWindowFlags_NoSavedSettings |
				ImGuiWindowFlags_NoInputs |
				ImGuiWindowFlags_AlwaysAutoResize |
				ImGuiWindowFlags_NoBackground);

			// HP Bar
			float hpRatio = player->GetHPRatio();
			float hp = player->GetHP();
			float maxHp = player->GetMaxHP();
			
			ImGui::SetWindowFontScale(1.4f);
			ImGui::TextColored(ImVec4(1.0f, 1.0f, 1.0f, 1.0f), "HP:");
			ImGui::SameLine();
			
			// Style colors for the health bar
			ImGui::PushStyleColor(ImGuiCol_PlotHistogram, ImVec4(0.8f, 0.1f, 0.1f, 1.0f)); // Red bar
			ImGui::PushStyleColor(ImGuiCol_FrameBg, ImVec4(0.15f, 0.15f, 0.15f, 0.6f));    // Dark background
			
			char hpText[32];
			std::snprintf(hpText, sizeof(hpText), "%.0f / %.0f", hp, maxHp);
			ImGui::ProgressBar(hpRatio, ImVec2(200.0f, 20.0f), hpText);
			ImGui::PopStyleColor(2);

			ImGui::Spacing();

			// Ammo
			if (player->IsReloading())
			{
				ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(1.0f, 0.8f, 0.0f, 1.0f));
				ImGui::Text("AMMO: RELOADING... %.1fs", (1.5f * (1.0f - player->GetReloadProgress())));
				ImGui::PopStyleColor();
			}
			else
			{
				int ammo = player->GetMagazineAmmo();
				int maxAmmo = player->GetMaxMagazineAmmo();
				if (ammo == 0)
				{
					ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(1.0f, 0.0f, 0.0f, 1.0f));
					ImGui::Text("AMMO: OUT OF AMMO! (Press R)");
					ImGui::PopStyleColor();
				}
				else
				{
					ImGui::Text("AMMO: %d / %d", ammo, maxAmmo);
				}
			}
			ImGui::End();

			// Stamina Bar (Rendered floating under the Player model)
			if (camera_)
			{
				Vector3 playerPos = player->GetPosition();
				
				// Project player 3D position to 2D screen space
				Matrix4x4 vp = Multiply(camera_->GetViewMatrix(), camera_->GetProjectionMatrix());
				
				// Transform: clip = (playerPos.x, playerPos.y, playerPos.z, 1.0f) * vp
				float cx = playerPos.x * vp.m[0][0] + playerPos.y * vp.m[1][0] + playerPos.z * vp.m[2][0] + vp.m[3][0];
				float cy = playerPos.x * vp.m[0][1] + playerPos.y * vp.m[1][1] + playerPos.z * vp.m[2][1] + vp.m[3][1];
				float cz = playerPos.x * vp.m[0][2] + playerPos.y * vp.m[1][2] + playerPos.z * vp.m[2][2] + vp.m[3][2];
				float cw = playerPos.x * vp.m[0][3] + playerPos.y * vp.m[1][3] + playerPos.z * vp.m[2][3] + vp.m[3][3];
				
				if (cw > 0.0f)
				{
					// NDC coordinates
					float ndcX = cx / cw;
					float ndcY = cy / cw;
					
					// Screen coordinates
					float screenX = (ndcX + 1.0f) * 0.5f * io.DisplaySize.x;
					float screenY = (1.0f - ndcY) * 0.5f * io.DisplaySize.y;
					
					// Floating Stamina HUD (centered below the player, say 50px below the center)
					ImGui::SetNextWindowPos(ImVec2(screenX - 50.0f, screenY + 40.0f), ImGuiCond_Always);
					ImGui::SetNextWindowSize(ImVec2(100.0f, 12.0f));
					ImGui::Begin("Floating Stamina HUD", nullptr,
						ImGuiWindowFlags_NoTitleBar |
						ImGuiWindowFlags_NoResize |
						ImGuiWindowFlags_NoMove |
						ImGuiWindowFlags_NoScrollbar |
						ImGuiWindowFlags_NoSavedSettings |
						ImGuiWindowFlags_NoInputs |
						ImGuiWindowFlags_NoBackground);
					
					float staminaRatio = player->GetStaminaRatio();
					
					// Small progress bar with no text
					ImGui::PushStyleColor(ImGuiCol_PlotHistogram, ImVec4(0.1f, 0.8f, 0.2f, 0.8f)); // Semi-transparent green
					ImGui::PushStyleColor(ImGuiCol_FrameBg, ImVec4(0.15f, 0.15f, 0.15f, 0.4f));    // Semi-transparent dark bg
					
					ImGui::ProgressBar(staminaRatio, ImVec2(100.0f, 6.0f), "");
					ImGui::PopStyleColor(2);
					ImGui::End();
				}
			}
		}

		if (timer < 5.0f) // プレイヤーがゾーン内にいてカウントダウン中の場合
		{
			ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(1.0f, 0.5f, 0.0f, 1.0f)); // オレンジ色
			ImGui::SetWindowFontScale(2.0f);
			ImGui::Text("EXTRACTING IN %.1fs...", timer);
			ImGui::PopStyleColor();
		}
		else
		{
			// 通常プレイ時
			ImGui::SetWindowFontScale(1.2f);
			ImGui::TextColored(ImVec4(1.0f, 1.0f, 1.0f, 0.6f), "Goal (Extraction Point): Go to the Ring primitive!");
		}

		// デバッグ表示：現在のプレイヤー座標とゴール座標、距離を表示
		Vector3 pPos = gameplay->GetPlayerPosition();
		Vector3 goalPos = gameplay->GetGoalPosition();
		float dx = pPos.x - goalPos.x;
		float dz = pPos.z - goalPos.z;
		float dist = std::sqrt(dx * dx + dz * dz);
		
		ImGui::Spacing();
		ImGui::SetWindowFontScale(1.0f);
		ImGui::TextColored(ImVec4(0.8f, 0.8f, 0.8f, 0.8f), "Player: (%.2f, %.2f, %.2f) | Goal: (%.2f, %.2f, %.2f)", pPos.x, pPos.y, pPos.z, goalPos.x, goalPos.y, goalPos.z);
		ImGui::TextColored(ImVec4(0.8f, 0.8f, 0.8f, 0.8f), "Distance: %.2f / 1.50 (kExtractionRadius)", dist);

		ImGui::End();
	}

	// クリアシーンの表示
	if (currentScene && std::string(currentScene->GetSceneType()) == "CLEAR")
	{
		ImGuiIO& io = ImGui::GetIO();

		// クリア表示ウィンドウの設定
		ImGui::SetNextWindowPos(ImVec2(io.DisplaySize.x * 0.5f, io.DisplaySize.y * 0.4f), ImGuiCond_Always, ImVec2(0.5f, 0.5f));
		ImGui::Begin("Clear Status", nullptr, 
			ImGuiWindowFlags_NoTitleBar | 
			ImGuiWindowFlags_NoResize | 
			ImGuiWindowFlags_NoMove | 
			ImGuiWindowFlags_NoScrollbar | 
			ImGuiWindowFlags_NoSavedSettings | 
			ImGuiWindowFlags_NoInputs | 
			ImGuiWindowFlags_AlwaysAutoResize | 
			ImGuiWindowFlags_NoBackground);

		ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.0f, 1.0f, 0.0f, 1.0f)); // 緑色
		ImGui::SetWindowFontScale(3.5f);
		ImGui::Text("★ GAME CLEAR ★");
		ImGui::PopStyleColor();

		ImGui::SetWindowFontScale(1.8f);
		ImGui::TextColored(ImVec4(0.9f, 0.9f, 0.9f, 1.0f), "Successfully Extracted from Dakkofu!");
		
		ImGui::SetWindowFontScale(1.3f);
		ImGui::TextColored(ImVec4(0.6f, 0.6f, 0.6f, 1.0f), "Press SPACE to return to Title");

		ImGui::End();
	}

	// ゲームオーバーシーンの表示
	if (currentScene && std::string(currentScene->GetSceneType()) == "GAMEOVER")
	{
		ImGuiIO& io = ImGui::GetIO();

		// ゲームオーバー表示ウィンドウの設定
		ImGui::SetNextWindowPos(ImVec2(io.DisplaySize.x * 0.5f, io.DisplaySize.y * 0.4f), ImGuiCond_Always, ImVec2(0.5f, 0.5f));
		ImGui::Begin("GameOver Status", nullptr, 
			ImGuiWindowFlags_NoTitleBar | 
			ImGuiWindowFlags_NoResize | 
			ImGuiWindowFlags_NoMove | 
			ImGuiWindowFlags_NoScrollbar | 
			ImGuiWindowFlags_NoSavedSettings | 
			ImGuiWindowFlags_NoInputs | 
			ImGuiWindowFlags_AlwaysAutoResize | 
			ImGuiWindowFlags_NoBackground);

		ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(1.0f, 0.0f, 0.0f, 1.0f)); // 赤色
		ImGui::SetWindowFontScale(3.5f);
		ImGui::Text("★ GAME OVER ★");
		ImGui::PopStyleColor();

		ImGui::SetWindowFontScale(1.8f);
		ImGui::TextColored(ImVec4(0.9f, 0.9f, 0.9f, 1.0f), "You Died!");
		
		ImGui::SetWindowFontScale(1.3f);
		ImGui::TextColored(ImVec4(0.6f, 0.6f, 0.6f, 1.0f), "Press SPACE to return to Title");

		ImGui::End();
	}
#endif
}

void DebugUI::Finalize()
{
}