#pragma once
#pragma once

#include "RenderContext.h"

struct SceneRenderRequests;

class SphereRenderer
{
public:
	void Draw(const RenderContext& ctx, const SceneRenderRequests& renderRequests) const;
};
