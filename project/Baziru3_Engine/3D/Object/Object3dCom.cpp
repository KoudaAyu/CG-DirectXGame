#include "Object3dCom.h"
#include "Baziru3_Engine/Base/Pipeline/PipelineStateManager.h"
#include "SceneManager.h"
#include "TextureManager.h"
#include "Light.h"
#include "Log.h"
#include <cassert>

Object3dCom::Object3dCom(std::ostream& logStream)
	: logStream(logStream)
{
}

void Object3dCom::Initialize(DirectXCom* directXCom)
{
	dxCommon = directXCom;
}

void Object3dCom::Update()
{
}

void Object3dCom::PreDraw()
{
	auto CommandList = dxCommon->GetCommandList();
	CommandList->SetGraphicsRootSignature(GetRootSignature().Get());
	CommandList->SetPipelineState(GetPipelineState().Get());
}

void Object3dCom::Draw(Object3d* object, const RenderContext& ctx, const Object3d::ModelData& modelData, bool drawObject)
{
    if (!ctx.commandList) return;
    if (!ctx.camera)
	{
		Logger::Log(logStream, "Warning: camera is null when drawing object. Skipping draw.\n");
		return;
	}

	ctx.commandList->SetGraphicsRootSignature(GetRootSignature().Get());
	ctx.commandList->SetPipelineState(GetPipelineState().Get());
   
    if (ctx.textureHandle.ptr != 0)
    {
        ctx.commandList->SetGraphicsRootDescriptorTable(2, ctx.textureHandle);
    }

    uint32_t skyboxIndex = SceneManager::GetInstance()->GetSkyboxTextureIndex();
    D3D12_GPU_DESCRIPTOR_HANDLE skyboxHandle{};
    if (skyboxIndex != TextureManager::kInvalidTextureIndex)
    {
        skyboxHandle = TextureManager::GetInstance()->GetSrvHandleGPU(skyboxIndex);
    }
    else
    {
        skyboxHandle = ctx.textureHandle;
    }
    if (skyboxHandle.ptr != 0)
    {
        ctx.commandList->SetGraphicsRootDescriptorTable(5, skyboxHandle);
    }

    if (ctx.light)
    {
        ctx.commandList->SetGraphicsRootConstantBufferView(3, ctx.light->GetDirectionalLightResource()->GetGPUVirtualAddress());
    }
    else
    {
        ctx.commandList->SetGraphicsRootConstantBufferView(3, 0);
    }

    if (ctx.camera->GetCameraResource())
    {
        ctx.commandList->SetGraphicsRootConstantBufferView(4, ctx.camera->GetCameraResource()->GetGPUVirtualAddress());
    }
    else
    {
        Logger::Log(logStream, "Warning: camera GPU resource not available when drawing object. Skipping draw.\n");
        return;
    }

	if (object)
	{
		object->Draw(ctx.commandList);
	}
}

const Microsoft::WRL::ComPtr<ID3D12RootSignature>& Object3dCom::GetRootSignature() const
{
	return PipelineStateManager::GetInstance()->GetRootSignature("Object3D");
}

const Microsoft::WRL::ComPtr<ID3D12PipelineState>& Object3dCom::GetPipelineState() const
{
	return PipelineStateManager::GetInstance()->GetPipelineState("Object3D_Normal");
}

const Microsoft::WRL::ComPtr<ID3D12PipelineState>& Object3dCom::GetEffectPipelineState() const
{
	return PipelineStateManager::GetInstance()->GetPipelineState("Object3D_Effect");
}

const Microsoft::WRL::ComPtr<ID3D12PipelineState>& Object3dCom::GetOverlayPipelineState() const
{
	return PipelineStateManager::GetInstance()->GetPipelineState("Object3D_Overlay");
}
