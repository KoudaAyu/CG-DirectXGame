#pragma once

namespace RootParam
{

    namespace Object3D {
        enum {
            kMaterial = 0,
            kTransform = 1,
			kEnvironmentTextureTable = 2,
			kTextureTable = 3,
			kLight = 4,
			kCamera = 5,
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