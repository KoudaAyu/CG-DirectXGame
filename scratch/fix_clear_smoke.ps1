$file = (Resolve-Path "project\Application\Scene\ClearScene\ClearScene.cpp").Path
$content = [System.IO.File]::ReadAllText($file, [System.Text.Encoding]::UTF8)

$oldBlock = @'
			// B. スモークグレネード特有の全方位360度ラジアル煙幕（四方八方へ地面を舐めて広がる分厚い煙幕壁）
			AppParticle smokeP{};
			smokeP.transform.Initialize();
			smokeP.transform.SetTranslate(ventPos);

			float initScale = 0.12f + (std::rand() % 100) * 0.0006f;
			smokeP.transform.SetScale({ initScale, initScale, 1.0f });
			smokeP.transform.SetRotate({ 0.0f, 0.0f, (std::rand() % 628) * 0.01f });

			// 水平360度へ勢いよく吹き散る高圧ラジアル噴出 ＋ 緩やかな上昇
			float rAngle = (std::rand() % 628) * 0.01f;
			float rSpeed = 1.30f + (std::rand() % 100) * 0.012f;
			float driftX = std::cos(rAngle) * rSpeed - 0.12f;
			float driftY = 0.35f + ((std::rand() % 100) * 0.008f);
			float driftZ = std::sin(rAngle) * rSpeed + 0.12f;
			smokeP.velocity = Vector3{ driftX, driftY, driftZ };

			// 濃密なミリタリー・タクティカルグリーンの煙幕
			smokeP.color = Vector4{ 0.12f, 1.0f, 0.36f, 0.95f };
			smokeP.endColor = Vector4{ 0.06f, 0.72f, 0.22f, 0.0f };
			smokeP.hasColorShift = true;

			smokeP.lifeTime = 2.8f + (std::rand() % 100) * 0.006f;
			smokeP.currentTime = 0.0f;
			smokeP.textureIndex = (smokeTexIndex_ != UINT32_MAX) ? smokeTexIndex_ : 0;

			smokeP.drag = 0.42f;
			smokeP.initialScale = initScale;
			smokeP.targetScale = 1.50f + (std::rand() % 100) * 0.004f;
			smokeP.wobbleFreq = 3.0f;
			smokeP.wobbleAmp = 0.18f;
			smokeP.wobbleAxis = { std::sin(rAngle), 0.3f, std::cos(rAngle) };
			smokeP.curlFreq = 2.2f;
			smokeP.curlAmp = 0.20f;

			appParticleManager_->AddParticle(smokeP);

			// C. 地面すれすれを全方位に這い回る重いスモークスクリーン・カーペット
			if ((sSmokeIndex % 2) == 0)
			{
				AppParticle groundSmokeP{};
				groundSmokeP.transform.Initialize();
				groundSmokeP.transform.SetTranslate(ventPos + Vector3{ 0.0f, -0.05f, 0.0f });

				float gInitScale = 0.14f;
				groundSmokeP.transform.SetScale({ gInitScale, gInitScale, 1.0f });
				groundSmokeP.transform.SetRotate({ 0.0f, 0.0f, (std::rand() % 628) * 0.01f });

				float gAngle = (std::rand() % 628) * 0.01f;
				float gSpeed = 1.10f + (std::rand() % 100) * 0.008f;
				groundSmokeP.velocity = Vector3{
					std::cos(gAngle) * gSpeed - 0.10f,
					0.08f + ((std::rand() % 100) * 0.0006f),
					std::sin(gAngle) * gSpeed + 0.10f
				};

				groundSmokeP.color = Vector4{ 0.10f, 0.95f, 0.32f, 0.90f };
				groundSmokeP.endColor = Vector4{ 0.05f, 0.65f, 0.18f, 0.0f };
				groundSmokeP.hasColorShift = true;

				groundSmokeP.lifeTime = 2.5f + (std::rand() % 100) * 0.005f;
				groundSmokeP.currentTime = 0.0f;
				groundSmokeP.textureIndex = (smokeTexIndex_ != UINT32_MAX) ? smokeTexIndex_ : 0;

				groundSmokeP.drag = 0.45f;
				groundSmokeP.initialScale = gInitScale;
				groundSmokeP.targetScale = 1.35f + (std::rand() % 100) * 0.004f;
				groundSmokeP.wobbleFreq = 2.4f;
				groundSmokeP.wobbleAmp = 0.14f;

				appParticleManager_->AddParticle(groundSmokeP);
			}
'@

$newBlock = @'
			// B. スモークグレネード：全方位360度へ極小粒子を大量バースト射出（8個/フレーム）
			for (int k = 0; k < 8; ++k)
			{
				AppParticle smokeP{};
				smokeP.transform.Initialize();
				smokeP.transform.SetTranslate(ventPos + Vector3{
					((std::rand() % 100) - 50) * 0.0004f,
					((std::rand() % 100) - 50) * 0.0003f,
					((std::rand() % 100) - 50) * 0.0004f
				});

				// 極小マイクロ粒子（初期 0.006m〜0.012m：6mm〜12mm）
				float initScale = 0.006f + (std::rand() % 100) * 0.00006f;
				smokeP.transform.SetScale({ initScale, initScale, 1.0f });
				smokeP.transform.SetRotate({ 0.0f, 0.0f, (std::rand() % 628) * 0.01f });

				// 水平360度＋斜め上へ吹き散る高圧ラジアル噴流
				float rAngle = (std::rand() % 628) * 0.01f;
				float rSpeed = 1.50f + (std::rand() % 100) * 0.015f; // 初速 1.5〜3.0 m/s
				float driftX = std::cos(rAngle) * rSpeed - 0.10f;
				float driftY = 0.22f + ((std::rand() % 100) * 0.008f); // 低空ドーム形成
				float driftZ = std::sin(rAngle) * rSpeed + 0.10f;
				smokeP.velocity = Vector3{ driftX, driftY, driftZ };

				// 濃密なタクティカルグリーンの細かな丸
				smokeP.color = Vector4{ 0.12f, 1.0f, 0.36f, 0.95f };
				smokeP.endColor = Vector4{ 0.06f, 0.72f, 0.22f, 0.0f };
				smokeP.hasColorShift = true;

				smokeP.lifeTime = 0.85f + (std::rand() % 100) * 0.004f;
				smokeP.currentTime = 0.0f;
				smokeP.textureIndex = (smokeTexIndex_ != UINT32_MAX) ? smokeTexIndex_ : 0;

				smokeP.drag = 0.48f;
				smokeP.initialScale = initScale;
				// 成長しても極小（最大 0.018m〜0.034m：ピンポン球以下の微小球、以前の1.5mから大幅縮小）
				smokeP.targetScale = 0.018f + (std::rand() % 100) * 0.00016f;
				smokeP.wobbleFreq = 3.8f;
				smokeP.wobbleAmp = 0.10f;
				smokeP.wobbleAxis = { std::sin(rAngle), 0.3f, std::cos(rAngle) };
				smokeP.curlFreq = 2.8f;
				smokeP.curlAmp = 0.12f;

				appParticleManager_->AddParticle(smokeP);
			}

			// C. 地面すれすれを全方位に這い回る極小粒子（5個/フレーム）
			for (int gk = 0; gk < 5; ++gk)
			{
				AppParticle groundSmokeP{};
				groundSmokeP.transform.Initialize();
				groundSmokeP.transform.SetTranslate(ventPos + Vector3{
					((std::rand() % 100) - 50) * 0.0004f,
					-0.08f,
					((std::rand() % 100) - 50) * 0.0004f
				});

				float gInitScale = 0.008f + (std::rand() % 100) * 0.00005f;
				groundSmokeP.transform.SetScale({ gInitScale, gInitScale, 1.0f });
				groundSmokeP.transform.SetRotate({ 0.0f, 0.0f, (std::rand() % 628) * 0.01f });

				float gAngle = (std::rand() % 628) * 0.01f;
				float gSpeed = 1.30f + (std::rand() % 100) * 0.012f;
				groundSmokeP.velocity = Vector3{
					std::cos(gAngle) * gSpeed - 0.08f,
					0.04f + ((std::rand() % 100) * 0.0003f), // 地面すれすれを水平に滑走
					std::sin(gAngle) * gSpeed + 0.08f
				};

				groundSmokeP.color = Vector4{ 0.10f, 0.95f, 0.32f, 0.92f };
				groundSmokeP.endColor = Vector4{ 0.05f, 0.65f, 0.18f, 0.0f };
				groundSmokeP.hasColorShift = true;

				groundSmokeP.lifeTime = 0.95f + (std::rand() % 100) * 0.003f;
				groundSmokeP.currentTime = 0.0f;
				groundSmokeP.textureIndex = (smokeTexIndex_ != UINT32_MAX) ? smokeTexIndex_ : 0;

				groundSmokeP.drag = 0.50f;
				groundSmokeP.initialScale = gInitScale;
				// 地面を這う極小粒子（最大 0.020m〜0.034m）
				groundSmokeP.targetScale = 0.020f + (std::rand() % 100) * 0.00014f;
				groundSmokeP.wobbleFreq = 3.0f;
				groundSmokeP.wobbleAmp = 0.08f;

				appParticleManager_->AddParticle(groundSmokeP);
			}
'@

# CRLF/LF の正規化を行って置換
$contentNormalized = $content.Replace("`r`n", "`n")
$oldNormalized = $oldBlock.Replace("`r`n", "`n")
$newNormalized = $newBlock.Replace("`r`n", "`n")

if ($contentNormalized.Contains($oldNormalized)) {
    $replaced = $contentNormalized.Replace($oldNormalized, $newNormalized)
    # 元のCRLFに戻して保存
    $replaced = $replaced.Replace("`n", "`r`n")
    [System.IO.File]::WriteAllText((Resolve-Path $file).Path, $replaced, [System.Text.Encoding]::UTF8)
    Write-Host "SUCCESS: ClearScene.cpp smoke replaced successfully!"
} else {
    Write-Error "ERROR: oldBlock not found in ClearScene.cpp!"
}
