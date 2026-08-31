#include "Object3dCom.h"
#include "Baziru3_Engine/Core/Base/Pipeline/PipelineStateManager.h"
#include "Baziru3_Engine/Framework/Collision/CollisionManager.h"
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
    static thread_local int s_callDepth = 0;
    struct DepthGuard {
        DepthGuard() { ++s_callDepth; }
        ~DepthGuard() { --s_callDepth; }
    } depthGuard;

    if (s_callDepth > 5)
    {
        OutputDebugStringA("[Object3dCom ERROR] Recursive Draw detected! Skipping to prevent Stack Overflow.\n");
        return;
    }

    if (!ctx.commandList) return;
    if (!ctx.camera)
	{
		Logger::Log(logStream, "Warning: camera is null when drawing object. Skipping draw.\n");
		return;
	}

	ctx.commandList->SetGraphicsRootSignature(GetRootSignature().Get());
	ctx.commandList->SetPipelineState(GetPipelineState().Get());
   
    D3D12_GPU_DESCRIPTOR_HANDLE mainTextureHandle = ctx.textureHandle;
    if (mainTextureHandle.ptr == 0)
    {
        mainTextureHandle = TextureManager::GetInstance()->GetSrvHandleGPU(
            TextureManager::GetInstance()->GetTextureIndexByFilePath("Resources/CG4/human/white.png"));
    }
    if (mainTextureHandle.ptr != 0)
    {
        ctx.commandList->SetGraphicsRootDescriptorTable(2, mainTextureHandle);
    }

    uint32_t skyboxIndex = SceneManager::GetInstance()->GetSkyboxTextureIndex();
    D3D12_GPU_DESCRIPTOR_HANDLE skyboxHandle{};
    if (skyboxIndex != TextureManager::kInvalidTextureIndex)
    {
        skyboxHandle = TextureManager::GetInstance()->GetSrvHandleGPU(skyboxIndex);
    }
    else
    {
        skyboxHandle = mainTextureHandle;
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

    if (ctx.camera->GetCameraGpuAddress() != 0)
    {
        ctx.commandList->SetGraphicsRootConstantBufferView(4, ctx.camera->GetCameraGpuAddress());
    }
    else
    {
        Logger::Log(logStream, "Warning: camera GPU resource not available when drawing object. Skipping draw.\n");
        return;
	}

	if (object)
	{
		object->DrawInternal(ctx);

		// GPU-accelerated wireframe overlay draw (if enabled in Collision Debug panel)
		if (CollisionManager::GetInstance()->IsShowDebugColliders() && CollisionManager::GetInstance()->IsShowMeshWireframe())
		{
			bool drawWireframe = true;
			if (ctx.camera)
			{
				Vector3 camPos = ctx.camera->GetTranslate();
				Vector3 objPos = object->GetTranslate();
				float dx = objPos.x - camPos.x;
				float dy = objPos.y - camPos.y;
				float dz = objPos.z - camPos.z;
				float distSq = dx * dx + dy * dy + dz * dz;
				if (distSq > 40.0f * 40.0f) // Skip wireframe if further than 40 units
				{
					drawWireframe = false;
				}
			}

			if (drawWireframe)
			{
				auto wireframePSO = GetWireframePipelineState();
				if (wireframePSO)
				{
					ctx.commandList->SetPipelineState(wireframePSO.Get());
					object->DrawInternal(ctx);
				}
			}
		}
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

const Microsoft::WRL::ComPtr<ID3D12PipelineState>& Object3dCom::GetWireframePipelineState() const
{
	return PipelineStateManager::GetInstance()->GetPipelineState("Object3D_Wireframe");
}
