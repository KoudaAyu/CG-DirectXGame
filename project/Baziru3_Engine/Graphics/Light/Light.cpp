#include "Light.h"

void Light::Initialize(DirectXCom* dxCommon)
{
	directXCom = dxCommon;

	// LightGroup のサイズで定数バッファを作成
	lightResource = directXCom->CreateBufferResource(directXCom->GetDevice().Get(), sizeof(Object3d::LightGroup));

	// 常時マップしてCPU側ポインタを取得
	lightResource->Map(0, nullptr, reinterpret_cast<void**>(&lightData));

	// 平行光源の初期設定 (白色・斜め下・強さ1.0)
	lightData->directionalLight.color = { 1.0f, 1.0f, 1.0f, 1.0f };
	lightData->directionalLight.direction = { 0.0f, -1.0f, 0.0f };
	lightData->directionalLight.intensity = 1.0f;

	// 点光源の初期設定 (消灯状態)
	lightData->pointLight.color = { 1.0f, 1.0f, 1.0f, 1.0f };
	lightData->pointLight.position = { 0.0f, 0.0f, 0.0f };
	lightData->pointLight.intensity = 0.0f;
	lightData->pointLight.radius = 10.0f;
	lightData->pointLight.decay = 2.0f;
}

void Light::SetDirectionalLight(const Vector4& color, const Vector3& direction, float intensity)
{
	if (!lightData) return;
	lightData->directionalLight.color = color;
	lightData->directionalLight.direction = direction;
	lightData->directionalLight.intensity = intensity;
}

void Light::SetDirectionalLightColor(const Vector4& color)
{
	if (!lightData) return;
	lightData->directionalLight.color = color;
}

void Light::SetDirectionalLightDirection(const Vector3& direction)
{
	if (!lightData) return;
	lightData->directionalLight.direction = direction;
}

void Light::SetDirectionalLightIntensity(float intensity)
{
	if (!lightData) return;
	lightData->directionalLight.intensity = intensity;
}

void Light::SetPointLight(const Vector4& color, const Vector3& position, float intensity, float radius, float decay)
{
	if (!lightData) return;
	lightData->pointLight.color = color;
	lightData->pointLight.position = position;
	lightData->pointLight.intensity = intensity;
	lightData->pointLight.radius = radius;
	lightData->pointLight.decay = decay;
}

void Light::SetPointLightColor(const Vector4& color)
{
	if (!lightData) return;
	lightData->pointLight.color = color;
}

void Light::SetPointLightPosition(const Vector3& position)
{
	if (!lightData) return;
	lightData->pointLight.position = position;
}

void Light::SetPointLightIntensity(float intensity)
{
	if (!lightData) return;
	lightData->pointLight.intensity = intensity;
}

void Light::SetPointLightRadius(float radius)
{
	if (!lightData) return;
	lightData->pointLight.radius = radius;
}

void Light::SetPointLightDecay(float decay)
{
	if (!lightData) return;
	lightData->pointLight.decay = decay;
}

const Object3d::DirectionalLight& Light::GetDirectionalLightData() const
{
	static Object3d::DirectionalLight defaultDirLight{ {1.0f, 1.0f, 1.0f, 1.0f}, {0.0f, -1.0f, 0.0f}, 1.0f };
	return lightData ? lightData->directionalLight : defaultDirLight;
}

const Object3d::PointLight& Light::GetPointLightData() const
{
	static Object3d::PointLight defaultPointLight{ {1.0f, 1.0f, 1.0f, 1.0f}, {0.0f, 0.0f, 0.0f}, 0.0f, 10.0f, 2.0f };
	return lightData ? lightData->pointLight : defaultPointLight;
}

const Object3d::LightGroup& Light::GetLightData() const
{
	static Object3d::LightGroup defaultLightGroup{};
	return lightData ? *lightData : defaultLightGroup;
}
