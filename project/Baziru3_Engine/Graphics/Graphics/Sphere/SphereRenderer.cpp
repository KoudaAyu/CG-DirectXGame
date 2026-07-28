#include "SphereRenderer.h"

#include "Baziru3_Engine\Graphics\Graphics\SceneRenderRequests.h"
#include "Sphere.h"

void SphereRenderer::Draw(const RenderContext& ctx, const SceneRenderRequests& renderRequests) const
{
	const auto& requestedSpheres = renderRequests.spheres.GetRequestedSpheres();
	for (Sphere* sphere : requestedSpheres)
	{
		if (!sphere)
		{
			continue;
		}

		sphere->Draw(ctx.textureHandle);
	}
}
