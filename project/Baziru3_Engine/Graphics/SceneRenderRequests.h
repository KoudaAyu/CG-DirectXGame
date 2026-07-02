#pragma once

#include "Baziru3_Engine\Graphics\Sphere\SphereDrawRequests.h"

struct SceneRenderRequests
{

    SphereDrawRequests spheres;
    // Indicates whether a scene's Draw() was invoked to populate these requests.
    bool sceneDrawn = false;

};
