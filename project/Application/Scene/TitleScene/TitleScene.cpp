#include "TitleScene.h"

#include "DirectXCom.h"
#include "KeyInput.h"
#include "SceneManager.h"
#include "WindowsAPI.h"

#include "Baziru3_Engine/Core/IO/Mouse/MouseInput.h"
#include "Baziru3_Engine/Graphics/Graphics/SceneRenderRequests.h"

#include <algorithm>
#include <cmath>

#ifdef USE_IMGUI
#include <imgui.h>
#endif

namespace {

// ===================================================================
// 画像パス
// まだ画像を用意していなくてもエンジン側が白いダミーテクスチャに
// 差し替えてくれるので、レイアウト確認は白い四角のままできる（落ちない）
// ===================================================================
constexpr const char* kTexTitleLogo = "Resources/UI/Title/title_logo.png";

struct ButtonAsset
{
	const char* base;  // 通常時
	const char* light; // 点灯時
};

constexpr ButtonAsset kButtonAssets[] = {
	{"Resources/UI/Title/btn_start.png",  "Resources/UI/Title/btn_start_light.png" },
	{"Resources/UI/Title/btn_manual.png", "Resources/UI/Title/btn_manual_light.png"},
	{"Resources/UI/Title/btn_end.png",    "Resources/UI/Title/btn_end_light.png"   },
};

constexpr const char* kMenuLabels[] = {"START", "MANUAL", "END"};

// ===================================================================
// レイアウト（1280x720 基準・すべて「中心座標」で指定）
// 画像を作るときはここのサイズに合わせればそのまま入る
// ===================================================================
constexpr Vector2 kTitleLogoSize = {640.0f, 160.0f};
constexpr Vector2 kTitleLogoCenter = {340.0f, 110.0f};
constexpr Vector2 kTitleLogoCenterInit = { 340.0f, 0.0f };

constexpr Vector2 kButtonSize = {320.0f, 90.0f};
constexpr float kButtonCenterX = 1050.0f; // ボタン列の中心X
constexpr float kButtonCenterXInit = 1250.0f;
constexpr float kButtonFirstY = 420.0f;  // 一番上（START）の中心Y
constexpr float kButtonStepY = 110.0f;   // ボタン同士の間隔

Vector2 TitleLogoCenter;
float ButtonCenterX[3];

// ===================================================================
// 演出パラメータ
// ===================================================================
constexpr float kDeltaTime = 1.0f / 60.0f; // SceneManager が固定タイムステップで回している
constexpr float kFadeInSeconds = 2.0f;     // 起動時フェードインの長さ（秒）
constexpr float kLogoBobAmplitude = 5.0f;  // ロゴの上下ゆれ幅（ピクセル）
constexpr float kLogoBobSpeed = 1.6f;      // ロゴの上下ゆれ速度（rad/秒）
constexpr float kButtonSwingSpeed = 1.2f;
constexpr float kButtonSwingAmplitude = 5.0f;
constexpr float kHoverScale = 1.08f;       // ホバー時の拡大率
constexpr float kHoverLerpRate = 12.0f;    // 拡縮の追従速度（1/秒）
constexpr float kBlinkSpeed = 5.0f;        // ライト点滅の速度（rad/秒）

/// <summary>指数補間でなめらかに目標値へ寄せる</summary>
float Approach(float current, float target, float rate, float deltaTime)
{
	const float t = 1.0f - std::exp(-rate * deltaTime);
	return current + (target - current) * t;
}

} // namespace

// ===================================================================
// 初期化 / 終了
// ===================================================================

void TitleScene::InitializeScene()
{
	if (dxCommon_)
	{
		input_ = new KeyInput();
		input_->Initialize(dxCommon_->GetWindowAPI());

		mouse_ = new MouseInput();
		mouse_->Initialize(dxCommon_->GetWindowAPI());
	}

	CreateSprites();

	sceneTime_ = 0.0f;
	fadeAlpha_ = 0.0f;
	logoOffsetY_ = 0.0f;
	ClearRequests();
}

void TitleScene::Finalize()
{
	if (titleLogo_)
	{
		titleLogo_->Finalize();
		titleLogo_.reset();
	}

	for (Button& button : buttons_)
	{
		if (button.base)
		{
			button.base->Finalize();
			button.base.reset();
		}
		if (button.light)
		{
			button.light->Finalize();
			button.light.reset();
		}
	}

	delete input_;
	input_ = nullptr;

	delete mouse_;
	mouse_ = nullptr;
}

void TitleScene::CreateSprites()
{
	titleLogo_ = MakeSprite(kTexTitleLogo, kTitleLogoSize);

	for (int i = 0; i < static_cast<int>(MenuItem::Count); ++i)
	{
		Button& button = buttons_[i];

		button.size = kButtonSize;
		button.center = {kButtonCenterX, kButtonFirstY + kButtonStepY * static_cast<float>(i)};
		button.scale = 1.0f;
		button.lightRate = 0.0f;
		button.isHovered = false;

		button.base = MakeSprite(kButtonAssets[i].base, kButtonSize);
		button.light = MakeSprite(kButtonAssets[i].light, kButtonSize);
	}
}

std::unique_ptr<Sprite> TitleScene::MakeSprite(const char* texturePath, const Vector2& size)
{
	std::unique_ptr<Sprite> sprite = Sprite::Create(texturePath, {0.0f, 0.0f});
	if (!sprite)
	{
		return nullptr;
	}

	// アンカーを中心にしておくと、拡縮が中心から効くので位置合わせが楽
	sprite->SetAnchorPoint({0.5f, 0.5f});
	sprite->SetSize(size);
	sprite->SetColor({1.0f, 1.0f, 1.0f, 0.0f}); // フェードインするので最初は透明
	return sprite;
}

// ===================================================================
// 更新
// ===================================================================

void TitleScene::Update()
{
	const float deltaTime = kDeltaTime;
	sceneTime_ += deltaTime;

	if (input_)
	{
		input_->Update();
	}
	if (mouse_)
	{
		mouse_->Update();
	}

	UpdateFadeIn(deltaTime);
	UpdateTitleLogo(deltaTime);
	UpdateButtons(deltaTime);

	// Space キーでもゲーム開始（従来のショートカットを残しておく）
	if (input_ && input_->TriggerKey(DIK_SPACE))
	{
		DecideMenu(MenuItem::Start);
	}

#ifdef USE_IMGUI
	ImGui::Begin("Title Scene");
	ImGui::Text("FadeIn : %.2f", fadeAlpha_);

	const Vector2 mousePos = GetMousePositionOnUI();
	ImGui::Text("Mouse  : %.0f, %.0f", mousePos.x, mousePos.y);
	ImGui::Separator();

	for (int i = 0; i < static_cast<int>(MenuItem::Count); ++i)
	{
		const Button& button = buttons_[i];
		ImGui::Text("%-7s hover=%d scale=%.2f light=%.2f", kMenuLabels[i],
					button.isHovered ? 1 : 0, button.scale, button.lightRate);
	}

	ImGui::Separator();
	ImGui::Text("Manual requested : %d", isManualRequested_ ? 1 : 0);
	ImGui::Text("Exit requested   : %d", isExitRequested_ ? 1 : 0);
	ImGui::End();
#endif
}

void TitleScene::UpdateFadeIn(float deltaTime)
{
	if (fadeAlpha_ < 1.0f)
	{		
		fadeAlpha_ = std::clamp(fadeAlpha_ + deltaTime / kFadeInSeconds, 0.0f, 1.0f);

		auto easing = [](float x)->float { return 1.0f - (1.0f - x) * (1.0f - x) * (1.0f - x); };
		{
			float const eased = easing(fadeAlpha_);
			TitleLogoCenter.x = kTitleLogoCenterInit.x * (1.0f - eased) + kTitleLogoCenter.x * eased;
			TitleLogoCenter.y = kTitleLogoCenterInit.y * (1.0f - eased) + kTitleLogoCenter.y * eased;
		}
		for (int i = 0; i < static_cast<int>(MenuItem::Count); ++i)
		{
			Button& button = buttons_[i];
			float const eased = std::clamp(easing(fadeAlpha_ * (1.5f - 0.25f * i)), 0.0f, 1.0f);
			ButtonCenterX[i] = kButtonCenterXInit * (1.0f - eased) + kButtonCenterX * eased;
		}
	}
}

void TitleScene::UpdateTitleLogo(float /*deltaTime*/)
{
	// ふわふわ上下に揺らす
	logoOffsetY_ = std::sin(sceneTime_ * kLogoBobSpeed) * kLogoBobAmplitude;

	const Vector2 center = { TitleLogoCenter.x, TitleLogoCenter.y + logoOffsetY_};
	ApplySprite(titleLogo_.get(), center, kTitleLogoSize, fadeAlpha_);
}

void TitleScene::UpdateButtons(float deltaTime)
{
	for (int i = 0; i < static_cast<int>(MenuItem::Count); ++i)
	{
		Button& button = buttons_[i];
		float offsetX = std::sin(sceneTime_ * (kButtonSwingSpeed + i * 0.5f)) * kButtonSwingAmplitude;
		button.center.x = ButtonCenterX[i] + offsetX;
	}

	const Vector2 mousePos = GetMousePositionOnUI();

	// フェードイン中は誤爆防止で入力を受け付けない
	const bool acceptInput = (fadeAlpha_ >= 1.0f);
	const bool isClicked = acceptInput && mouse_ && mouse_->TriggerButton(0);

	for (int i = 0; i < static_cast<int>(MenuItem::Count); ++i)
	{
		Button& button = buttons_[i];

		button.isHovered = acceptInput && IsInside(mousePos, button.center, button.size);

		// ホバーで軽く拡縮
		const float targetScale = button.isHovered ? kHoverScale : 1.0f;
		button.scale = Approach(button.scale, targetScale, kHoverLerpRate, deltaTime);

		// ライトの点滅：ホバー中は sin で 0..1 を往復、外れたらなめらかに消灯
		if (button.isHovered)
		{
			button.lightRate = (std::sin(sceneTime_ * kBlinkSpeed) * 0.5f) + 0.5f;
		}
		else
		{
			button.lightRate = Approach(button.lightRate, 0.0f, kHoverLerpRate, deltaTime);
		}

		const Vector2 drawSize = {button.size.x * button.scale, button.size.y * button.scale};

		// 下に通常画像、その上に点灯画像を重ねて不透明度で入れ替える。
		// 下の画像も一緒に薄くする本来のクロスフェードにしたい場合は、
		// 下の行の fadeAlpha_ を fadeAlpha_ * (1.0f - button.lightRate) に変える
		ApplySprite(button.base.get(), button.center, drawSize, fadeAlpha_);
		ApplySprite(button.light.get(), button.center, drawSize, fadeAlpha_ * button.lightRate);

		if (button.isHovered && isClicked)
		{
			DecideMenu(static_cast<MenuItem>(i));
		}
	}
}

void TitleScene::DecideMenu(MenuItem item)
{
	switch (item)
	{
	case MenuItem::Start:
		SceneManager::GetInstance()->ChangeScene("GAMEPLAY");
		break;

	case MenuItem::Manual:
		// TODO: マニュアル画面ができたらここで遷移させる
		isManualRequested_ = true;
		break;

	case MenuItem::End:
		// TODO: 終了処理（PostQuitMessage など）をここに入れる
		isExitRequested_ = true;
		break;

	default:
		break;
	}
}

// ===================================================================
// 描画
// ===================================================================

void TitleScene::Draw(SceneRenderRequests& /*renderRequests*/)
{
	if (!dxCommon_ || !dxCommon_->GetCommandList())
	{
		return;
	}

	ID3D12GraphicsCommandList* commandList = dxCommon_->GetCommandList().Get();

	// 背景スカイボックスの描画
	SceneManager::GetInstance()->DrawSkybox(commandList);

	// UI の描画。Sprite::Draw() が内部で RootSignature / PSO を張り直すので、
	// ここで 2D を描いても後続の 3D 描画には影響しない
	if (titleLogo_)
	{
		titleLogo_->Draw(commandList);
	}

	for (const Button& button : buttons_)
	{
		if (button.base)
		{
			button.base->Draw(commandList);
		}
		// 完全に透明なときは描かない
		if (button.light && button.lightRate > 0.0f)
		{
			button.light->Draw(commandList);
		}
	}
}

// ===================================================================
// ヘルパー
// ===================================================================

Vector2 TitleScene::GetMousePositionOnUI() const
{
	// 仮想解像度（1280x720）上のマウス座標を返す。
	// MouseInput::GetScaledPosition() は先に仮想解像度でクランプしてから
	// スケールする実装なので、ウィンドウが 1280x720 より大きいとズレる。
	// ここでは実クライアント座標から自前で換算している。
	Vector2 result = {-1.0f, -1.0f};

	WindowAPI* windowAPI = dxCommon_ ? dxCommon_->GetWindowAPI() : nullptr;
	if (!windowAPI)
	{
		return result;
	}

	const HWND hwnd = windowAPI->GetHwnd();
	POINT point{};
	RECT clientRect{};
	if (!GetCursorPos(&point) || !ScreenToClient(hwnd, &point) ||
		!GetClientRect(hwnd, &clientRect))
	{
		return result;
	}

	const float clientWidth = static_cast<float>(clientRect.right - clientRect.left);
	const float clientHeight = static_cast<float>(clientRect.bottom - clientRect.top);
	if (clientWidth <= 0.0f || clientHeight <= 0.0f)
	{
		return result;
	}

	result.x = static_cast<float>(point.x) *
			   (static_cast<float>(WindowAPI::GetClientWidth()) / clientWidth);
	result.y = static_cast<float>(point.y) *
			   (static_cast<float>(WindowAPI::GetClientHeight()) / clientHeight);
	return result;
}

bool TitleScene::IsInside(const Vector2& point, const Vector2& center, const Vector2& size)
{
	const float halfWidth = size.x * 0.5f;
	const float halfHeight = size.y * 0.5f;

	return (point.x >= center.x - halfWidth) && (point.x <= center.x + halfWidth) &&
		   (point.y >= center.y - halfHeight) && (point.y <= center.y + halfHeight);
}

void TitleScene::ApplySprite(Sprite* sprite, const Vector2& center, const Vector2& size,
							 float alpha)
{
	if (!sprite)
	{
		return;
	}

	sprite->SetPosition(center);
	sprite->SetSize(size);

	Vector4 color = sprite->GetColor();
	color.w = std::clamp(alpha, 0.0f, 1.0f);
	sprite->SetColor(color);

	// 頂点・行列の書き込みはメインスレッド側（この Update）で済ませておく
	sprite->Update();
}
