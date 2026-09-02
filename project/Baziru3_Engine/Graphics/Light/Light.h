#pragma once
#include <d3d12.h>
#include<wrl.h>

#include"DirectXCom.h"
#include"Object3d.h"


class Light
{
public:
	void Initialize(DirectXCom* dxCommon);

	// 定数バッファリソース取得
	Microsoft::WRL::ComPtr<ID3D12Resource> GetLightResource() const { return lightResource; }
	Microsoft::WRL::ComPtr<ID3D12Resource> GetDirectionalLightResource() const { return lightResource; }

	// --- 平行光源 (Directional Light) の設定 ---
	void SetDirectionalLight(const Vector4& color, const Vector3& direction, float intensity);
	void SetDirectionalLightColor(const Vector4& color);
	void SetDirectionalLightDirection(const Vector3& direction);
	void SetDirectionalLightIntensity(float intensity);

	// --- 点光源 (Point Light) の設定 ---
	void SetPointLight(const Vector4& color, const Vector3& position, float intensity, float radius, float decay = 2.0f);
	void SetPointLightColor(const Vector4& color);
	void SetPointLightPosition(const Vector3& position);
	void SetPointLightIntensity(float intensity);
	void SetPointLightRadius(float radius);
	void SetPointLightDecay(float decay);

	// --- ゲッター ---
	const Object3d::DirectionalLight& GetDirectionalLightData() const;
	const Object3d::PointLight& GetPointLightData() const;
	const Object3d::LightGroup& GetLightData() const;

private:
	DirectXCom* directXCom = nullptr;

	Microsoft::WRL::ComPtr<ID3D12Resource> lightResource;
	Object3d::LightGroup* lightData = nullptr;
};

