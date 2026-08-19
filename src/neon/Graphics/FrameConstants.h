#pragma once

#include "neon-math.h"

namespace neon::gfx {

using HlslBool = int32_t; // For alignment on GPU

enum class TextureFilterMode {
    Point, EnhancedPoint, Smooth
};

struct FrameConstants {
    Matrix ViewProjection;
    Matrix View;
    Matrix Projection;
    Vector3 Eye;
    float ElapsedTime;
    Vector2 Size;
    float NearClip, FarClip;
    Vector3 EyeDir;
    float GlobalDimming;
    Vector3 EyeUp;
    HlslBool NewLightMode;
    TextureFilterMode FilterMode;
    float RenderScale;
};

}