#pragma once
#include "Camera.h"
#include "DirectXCom.h"
#include <d3d12.h>
#include <ostream>
#include "Object3d.h"
#include "RenderContext.h"

class Object3dCom
{
public:
	Object3dCom(std::ostream& logStream);
	~Object3dCom() = default;

	void Initialize(DirectXCom* directXCom);
	void Update();

	void PreDraw();

	void Draw(Object3d* object, const RenderContext& ctx, const Object3d::ModelData& modelData, bool drawObject);

public:
	void SetDefaultCamera(Camera* camera)
	{
		defaultCamera_ = camera;
	}
	Camera* GetDefaultCamera() const
	{
		return defaultCamera_;
	}
	DirectXCom* GetDirectXCom() const
	{
		return dxCommon;
	}
	const Microsoft::WRL::ComPtr<ID3D12PipelineState>& GetPipelineState() const;
	const Microsoft::WRL::ComPtr<ID3D12PipelineState>& GetEffectPipelineState() const;
	const Microsoft::WRL::ComPtr<ID3D12PipelineState>& GetOverlayPipelineState() const;
	const Microsoft::WRL::ComPtr<ID3D12RootSignature>& GetRootSignature() const;

private:
	Camera* defaultCamera_ = nullptr;
	DirectXCom* dxCommon = nullptr;
	std::ostream& logStream;
};
