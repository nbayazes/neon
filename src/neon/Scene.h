#pragma once
#include "neon-math.h"

namespace neon {

struct ModelInstance {
    int mesh; // mesh id
    DirectX::SimpleMath::Vector3 position;
    Matrix3x3 rotation;
};

struct Scene {
    std::vector<ModelInstance> models;
};

}
