#pragma once
#pragma once

#include <d3d12.h>

#include "RenderContext.h"

class Model;
class ParticleManager;

class ParticleRenderer
{
public:
    void Draw(const RenderContext& ctx, ParticleManager* particleManager, Model* model, UINT externalVertexCount) const;
};
