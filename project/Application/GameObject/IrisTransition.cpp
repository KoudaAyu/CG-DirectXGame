#include "IrisTransition.h"
#include <algorithm>
#include <cmath>
#include <iostream>

IrisTransition* IrisTransition::GetInstance()
{
    static IrisTransition instance;
    return &instance;
}

IrisTransition::~IrisTransition()
{
    Finalize();
}

void IrisTransition::Initialize(DirectXCom* dxCommon)
{
    if (!dxCommon)
    {
        return;
    }

    if (dxCommon_ == dxCommon && constantBuffer_ && pipelineState_)
    {
        state_ = State::Idle;
        progress_ = 0.0f;
        timer_ = 0.0f;
        return;
    }

    dxCommon_ = dxCommon;
    CreatePipelines(dxCommon);

    // 256バイトアライメントで定数バッファを生成
    size_t cbSize = (sizeof(IrisParamsCPU) + 255) & ~255;
    constantBuffer_ = dxCommon_->CreateBufferResource(dxCommon_->GetDevice(), cbSize);
    if (constantBuffer_)
    {
        constantBuffer_->Map(0, nullptr, reinterpret_cast<void**>(&mappedBuffer_));
        if (mappedBuffer_)
        {
            mappedBuffer_->center = { 0.5f, 0.5f };
            mappedBuffer_->radius = maxRadius_;
            float w = dxCommon_->GetViewport().Width;
            float h = dxCommon_->GetViewport().Height;
            mappedBuffer_->aspectRatio = (h > 0.0f) ? (w / h) : (16.0f / 9.0f);
            mappedBuffer_->feather = 0.015f;
        }
    }

    state_ = State::Idle;
    progress_ = 0.0f;
    timer_ = 0.0f;
}

void IrisTransition::Finalize()
{
    if (constantBuffer_)
    {
        if (mappedBuffer_)
        {
            constantBuffer_->Unmap(0, nullptr);
            mappedBuffer_ = nullptr;
        }
        constantBuffer_.Reset();
    }

    pipelineState_.Reset();
    rootSignature_.Reset();
    dxCommon_ = nullptr;
    state_ = State::Idle;
    progress_ = 0.0f;
}

void IrisTransition::CreatePipelines(DirectXCom* dxCommon)
{
    if (!dxCommon || !dxCommon->GetDevice())
    {
        return;
    }

    // シェーダーのコンパイル
    Microsoft::WRL::ComPtr<IDxcBlob> vsBlob = dxCommon->CompileShader(
        L"Resources/shaders/IrisTransition.VS.hlsl", L"vs_6_0",
        dxCommon->GetDxcUtils().Get(), dxCommon->GetDxcCompiler(), dxCommon->GetIncludeHandler(), std::cout);

    Microsoft::WRL::ComPtr<IDxcBlob> psBlob = dxCommon->CompileShader(
        L"Resources/shaders/IrisTransition.PS.hlsl", L"ps_6_0",
        dxCommon->GetDxcUtils().Get(), dxCommon->GetDxcCompiler(), dxCommon->GetIncludeHandler(), std::cout);

    if (!vsBlob || !psBlob)
    {
        OutputDebugStringA("IrisTransition: Failed to compile transition shaders.\n");
        return;
    }

    // RootSignature の作成 (CBV register b0)
    D3D12_ROOT_PARAMETER rootParam{};
    rootParam.ParameterType = D3D12_ROOT_PARAMETER_TYPE_CBV;
    rootParam.ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL;
    rootParam.Descriptor.ShaderRegister = 0;
    rootParam.Descriptor.RegisterSpace = 0;

    D3D12_ROOT_SIGNATURE_DESC rootDesc{};
    rootDesc.NumParameters = 1;
    rootDesc.pParameters = &rootParam;
    rootDesc.NumStaticSamplers = 0;
    rootDesc.pStaticSamplers = nullptr;
    rootDesc.Flags = D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT;

    Microsoft::WRL::ComPtr<ID3DBlob> signatureBlob;
    Microsoft::WRL::ComPtr<ID3DBlob> errorBlob;
    HRESULT hr = D3D12SerializeRootSignature(&rootDesc, D3D_ROOT_SIGNATURE_VERSION_1, &signatureBlob, &errorBlob);
    if (FAILED(hr))
    {
        if (errorBlob)
        {
            OutputDebugStringA(reinterpret_cast<const char*>(errorBlob->GetBufferPointer()));
        }
        return;
    }

    hr = dxCommon->GetDevice()->CreateRootSignature(
        0, signatureBlob->GetBufferPointer(), signatureBlob->GetBufferSize(), IID_PPV_ARGS(&rootSignature_));
    if (FAILED(hr))
    {
        OutputDebugStringA("IrisTransition: Failed to create RootSignature.\n");
        return;
    }

    // PSO の作成
    D3D12_GRAPHICS_PIPELINE_STATE_DESC psoDesc{};
    psoDesc.pRootSignature = rootSignature_.Get();
    psoDesc.InputLayout = { nullptr, 0 }; // フルスクリーントライアングル方式（頂点バッファなし）
    psoDesc.VS = { vsBlob->GetBufferPointer(), vsBlob->GetBufferSize() };
    psoDesc.PS = { psBlob->GetBufferPointer(), psBlob->GetBufferSize() };

    // 半透明アルファブレンド
    psoDesc.BlendState.RenderTarget[0].RenderTargetWriteMask = D3D12_COLOR_WRITE_ENABLE_ALL;
    psoDesc.BlendState.RenderTarget[0].BlendEnable = TRUE;
    psoDesc.BlendState.RenderTarget[0].SrcBlend = D3D12_BLEND_SRC_ALPHA;
    psoDesc.BlendState.RenderTarget[0].BlendOp = D3D12_BLEND_OP_ADD;
    psoDesc.BlendState.RenderTarget[0].DestBlend = D3D12_BLEND_INV_SRC_ALPHA;
    psoDesc.BlendState.RenderTarget[0].SrcBlendAlpha = D3D12_BLEND_ONE;
    psoDesc.BlendState.RenderTarget[0].BlendOpAlpha = D3D12_BLEND_OP_ADD;
    psoDesc.BlendState.RenderTarget[0].DestBlendAlpha = D3D12_BLEND_ZERO;

    psoDesc.RasterizerState.FillMode = D3D12_FILL_MODE_SOLID;
    psoDesc.RasterizerState.CullMode = D3D12_CULL_MODE_NONE; // カリングなし
    psoDesc.RasterizerState.FrontCounterClockwise = FALSE;
    psoDesc.RasterizerState.DepthClipEnable = TRUE;

    psoDesc.NumRenderTargets = 1;
    psoDesc.RTVFormats[0] = DXGI_FORMAT_R8G8B8A8_UNORM_SRGB;
    psoDesc.DSVFormat = DXGI_FORMAT_D24_UNORM_S8_UINT;
    psoDesc.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;
    psoDesc.SampleDesc.Count = 1;
    psoDesc.SampleMask = D3D12_DEFAULT_SAMPLE_MASK;

    // 画面全体への最前面描画のため深度テスト・書き込みは無効
    psoDesc.DepthStencilState.DepthEnable = FALSE;
    psoDesc.DepthStencilState.DepthWriteMask = D3D12_DEPTH_WRITE_MASK_ZERO;

    hr = dxCommon->GetDevice()->CreateGraphicsPipelineState(&psoDesc, IID_PPV_ARGS(&pipelineState_));
    if (FAILED(hr))
    {
        OutputDebugStringA("IrisTransition: Failed to create PipelineState.\n");
    }
}

void IrisTransition::StartIrisOut(float duration, const Vector2& centerUV)
{
    duration_ = (duration > 0.0f) ? duration : 1.0f;
    timer_ = 0.0f;
    centerUV_ = centerUV;
    state_ = State::IrisOut;
    progress_ = 0.0f;
}

void IrisTransition::StartIrisIn(float duration, const Vector2& centerUV)
{
    duration_ = (duration > 0.0f) ? duration : 1.0f;
    timer_ = 0.0f;
    centerUV_ = centerUV;
    state_ = State::IrisIn;
    progress_ = 1.0f;
}

void IrisTransition::Update(float deltaTime)
{
    float currentRadius = maxRadius_;

    switch (state_)
    {
    case State::Idle:
        progress_ = 0.0f;
        currentRadius = maxRadius_;
        break;

    case State::IrisOut:
    {
        timer_ += deltaTime;
        float t = std::clamp(timer_ / duration_, 0.0f, 1.0f);
        // smoothstep イージング
        float eased = t * t * (3.0f - 2.0f * t);
        progress_ = eased;
        currentRadius = maxRadius_ * (1.0f - eased);

        if (timer_ >= duration_)
        {
            state_ = State::Blackout;
            progress_ = 1.0f;
            currentRadius = 0.0f;
        }
        break;
    }

    case State::Blackout:
        progress_ = 1.0f;
        currentRadius = 0.0f;
        break;

    case State::IrisIn:
    {
        timer_ += deltaTime;
        float t = std::clamp(timer_ / duration_, 0.0f, 1.0f);
        float eased = t * t * (3.0f - 2.0f * t);
        progress_ = 1.0f - eased;
        currentRadius = maxRadius_ * eased;

        if (timer_ >= duration_)
        {
            state_ = State::Idle;
            progress_ = 0.0f;
            currentRadius = maxRadius_;
        }
        break;
    }
    }

    // 定数バッファの更新
    if (mappedBuffer_ && dxCommon_)
    {
        mappedBuffer_->center = centerUV_;
        mappedBuffer_->radius = currentRadius;
        float w = dxCommon_->GetViewport().Width;
        float h = dxCommon_->GetViewport().Height;
        mappedBuffer_->aspectRatio = (h > 0.0f) ? (w / h) : (16.0f / 9.0f);
        mappedBuffer_->feather = 0.015f;
    }
}

void IrisTransition::Draw(ID3D12GraphicsCommandList* commandList)
{
    if (!commandList || !pipelineState_ || !rootSignature_ || !constantBuffer_)
    {
        return;
    }

    // Idle状態で完全に開いている（progress_ <= 0）ときは何も描画せず負荷ゼロ
    if (state_ == State::Idle && progress_ <= 0.0f)
    {
        return;
    }

    if (dxCommon_)
    {
        commandList->RSSetViewports(1, &dxCommon_->GetViewport());
        commandList->RSSetScissorRects(1, &dxCommon_->GetScissorRect());
    }

    commandList->SetGraphicsRootSignature(rootSignature_.Get());
    commandList->SetPipelineState(pipelineState_.Get());
    commandList->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
    commandList->SetGraphicsRootConstantBufferView(0, constantBuffer_->GetGPUVirtualAddress());

    // 頂点バッファ不要のフルスクリーントライアングル描画（3頂点）
    commandList->DrawInstanced(3, 1, 0, 0);
}
