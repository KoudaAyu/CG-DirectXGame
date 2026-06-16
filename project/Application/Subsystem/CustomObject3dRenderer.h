#pragma once
#include <wrl.h>
#include <d3d12.h>
#include <dxcapi.h>
#include <ostream>
#include <iostream>
#include "DirectXCom.h"
#include "RenderContext.h"
#include "Object3d.h"

class CustomObject3dRenderer
{
public:
    static CustomObject3dRenderer* GetInstance();

    void Initialize(DirectXCom* dxCommon, std::ostream& logStream = std::cerr);
    void Finalize();

    void Draw(Object3d* object, const RenderContext& ctx, const Object3d::ModelData& modelData, bool drawObject);

private:
    CustomObject3dRenderer() = default;
    ~CustomObject3dRenderer() = default;
    CustomObject3dRenderer(const CustomObject3dRenderer&) = delete;
    CustomObject3dRenderer& operator=(const CustomObject3dRenderer&) = delete;

    void CreateRootSignature();
    void CreatePipelineState();

private:
    DirectXCom* dxCommon_ = nullptr;
    std::ostream* logStream_ = nullptr;

    Microsoft::WRL::ComPtr<ID3D12RootSignature> rootSignature_ = nullptr;
    Microsoft::WRL::ComPtr<ID3D12PipelineState> pipelineState_ = nullptr;
};
