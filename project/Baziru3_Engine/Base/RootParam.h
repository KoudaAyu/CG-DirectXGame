#pragma once

namespace RootParam
{

    namespace Object3D {
        enum {
            kMaterial = 0,
            kTransform = 1,
            kTextureTable = 2,
            kLight = 3,
            kCamera = 4,
            kPointLight = 5,
            kSpotLight = 6,
        };
    }

    namespace Particle {
        enum {
            kMaterial = 0,
            kInstancing = 1,
            kTextureTable = 2,
            kLight = 3,
            kCamera = 4,
        };
    }
}