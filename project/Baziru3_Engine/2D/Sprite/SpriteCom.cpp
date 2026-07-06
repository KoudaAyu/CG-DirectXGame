#include "SpriteCom.h"
#include "Baziru3_Engine/Base/Pipeline/PipelineStateManager.h"
#include <cassert>

SpriteCom::SpriteCom(std::ostream& logStream, DirectXCom* dxCommon)
	: logStream(logStream), dxCommon(dxCommon)
{
}

SpriteCom::~SpriteCom()
{
}

void SpriteCom::Initialize()
{
}

void SpriteCom::Finalize()
{
}

void SpriteCom::SetupDraw(ID3D12GraphicsCommandList* commandList)
{
	commandList->SetGraphicsRootSignature(GetRootSignature().Get());
	commandList->SetPipelineState(GetPipelineState().Get());
}

void SpriteCom::SetBlendMode(BlendMode mode)
{
	currentBlendMode = mode;
}

const Microsoft::WRL::ComPtr<ID3D12RootSignature>& SpriteCom::GetRootSignature() const
{
	return PipelineStateManager::GetInstance()->GetRootSignature("Sprite");
}

const Microsoft::WRL::ComPtr<ID3D12PipelineState>& SpriteCom::GetPipelineState() const
{
	std::string key = "Sprite_Normal";
	switch (currentBlendMode)
	{
	case kBlendMode_None:
		key = "Sprite_None";
		break;
	case kBlendMode_Normal:
		key = "Sprite_Normal";
		break;
	case kBlendMode_Add:
		key = "Sprite_Add";
		break;
	case kBlendMode_Sub:
		key = "Sprite_Subtract";
		break;
	case kBlendMode_Mul:
		key = "Sprite_Multiply";
		break;
	case kBlendMode_Screen:
		key = "Sprite_Screen";
		break;
	default:
		break;
	}
	return PipelineStateManager::GetInstance()->GetPipelineState(key);
}
