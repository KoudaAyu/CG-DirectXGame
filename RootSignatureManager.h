#pragma once
#include <d3d12.h>
#include <wrl.h>

#include "DebugLog.h"


class RootSignatureManager
{
public:
    void CreateDescriptionRootSignature();
    void CreateDescriptorRange();
    void CreateRootParemeter();
    void CreateStaticSamplers();
    void CreateBlob(const Microsoft::WRL::ComPtr<ID3D12Device>& device, HRESULT hr);

    const D3D12_ROOT_SIGNATURE_DESC& GetRootSignatureDesc() const
    {
        return descriptionRootSignature;
    }
    D3D12_ROOT_SIGNATURE_DESC& GetRootSignatureDesc()
    {
        return descriptionRootSignature;
    }

    const D3D12_DESCRIPTOR_RANGE* GetDescriptorRangePtr() const
    {
        return descriptorRange;
    }
    D3D12_DESCRIPTOR_RANGE* GetDescriptorRangePtr()
    {
        return descriptorRange;
    }
    size_t GetDescriptorRangeCount() const { return 1; }

    // 追加: rootParametersのgetter
    const D3D12_ROOT_PARAMETER* GetRootParametersPtr() const 
    { 
        return rootParameters;
    }
    D3D12_ROOT_PARAMETER* GetRootParametersPtr()
    {
        return rootParameters;
    }
    size_t GetRootParametersCount() const { return 4; }

	const Microsoft::WRL::ComPtr<ID3DBlob>& GetSignatureBlob() const
	{
		return signatureBlob;
	}

	const Microsoft::WRL::ComPtr<ID3DBlob>& GetErrorBlob() const
	{
		return errorBlob;
	}

	const Microsoft::WRL::ComPtr<ID3D12RootSignature>& GetRootSignature() const
	{
		return rootSignature;
	}
	


private:
    D3D12_ROOT_SIGNATURE_DESC descriptionRootSignature{};
    D3D12_DESCRIPTOR_RANGE descriptorRange[1]{};
    D3D12_ROOT_PARAMETER rootParameters[4] = {};
    D3D12_STATIC_SAMPLER_DESC staticSamplers[1] = {};
    Microsoft::WRL::ComPtr<ID3DBlob> signatureBlob = nullptr;
    Microsoft::WRL::ComPtr<ID3DBlob> errorBlob = nullptr;
    Microsoft::WRL::ComPtr<ID3D12RootSignature> rootSignature = nullptr;
    Debug debug;
};
