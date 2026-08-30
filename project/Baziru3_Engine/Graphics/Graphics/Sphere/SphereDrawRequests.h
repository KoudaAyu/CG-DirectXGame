#pragma once

#include <vector>

class Sphere;

class SphereDrawRequests
{
public:
    void Request(Sphere* sphere)
    {
        if (!sphere)
        {
            return;
        }

        requestedSpheres_.push_back(sphere);
    }

    const std::vector<Sphere*>& GetRequestedSpheres() const
    {
        return requestedSpheres_;
    }

    void Clear()
    {
        requestedSpheres_.clear();
    }

private:
    std::vector<Sphere*> requestedSpheres_;
};
