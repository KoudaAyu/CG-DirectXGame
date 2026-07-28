#include "ParticleRenderer.h"

#include "Camera.h"
#include "Light.h"
#include "Model.h"
#include "ParticleManager.h"
#include "RootParam.h"

void ParticleRenderer::Draw(const RenderContext& ctx, ParticleManager* particleManager, Model* model, UINT externalVertexCount) const
{
    if (!ctx.commandList || !particleManager)
    {
        return;
    }

    // パーティクル用ルートシグネチャとPSOを先に設定する
    particleManager->SetupDraw(ctx.commandList);

    if (ctx.light)
    {
        ctx.commandList->SetGraphicsRootConstantBufferView(
            RootParam::Particle::kLight,
            ctx.light->GetDirectionalLightResource()->GetGPUVirtualAddress());
    }
    else
    {
        ctx.commandList->SetGraphicsRootConstantBufferView(RootParam::Particle::kLight, 0);
    }

    ctx.commandList->SetGraphicsRootConstantBufferView(
        RootParam::Particle::kCamera,
        ctx.camera && ctx.camera->GetCameraResource()
            ? ctx.camera->GetCameraResource()->GetGPUVirtualAddress()
            : 0);

    if (particleManager->GetNumInstance() == 0)
    {
        return;
    }

    if (model && externalVertexCount > 0)
    {
        model->Bind(ctx.commandList);
        particleManager->Draw(ctx.commandList, ctx, externalVertexCount);
    }

    particleManager->Draw(ctx.commandList, ctx, 0);
}
