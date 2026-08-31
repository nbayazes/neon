#pragma once

#include "neon-types.h"
#include "neon-math.h"

namespace neon {
class StreamReader;
}

namespace neon::d3 {

constexpr auto MIN_OBJFILE_VERSION = 1807;
constexpr auto OBJFILE_VERSION = 2300;

enum class ModelFlag {
    None = 0,
    LightmapRes = (1 << 0),
    Timed = (1 << 1), // Uses timed animation
    Alpha = (1 << 2), // Has alpha per vertex qualities
    Facing = (1 << 3), // Has a submodel that is always facing
    NotResident = (1 << 4), // This polymodel is not in memory
    SizeComputed = (1 << 5), // This polymodel's size is computed
};

enum class SubmodelFlag {
    Rotate = (1 << 0), // This subobject is a rotator
    Turret = (1 << 1), // This subobject is a turret that tracks
    Shell = (1 << 2), // This subobject is a door housing
    Frontface = (1 << 3), // This subobject contains the front face for the door
    Monitor1 = (1 << 4), // cockpit monitor
    Monitor2 = (1 << 5), // cockpit monitor
    Monitor3 = (1 << 6), // cockpit monitor
    Monitor4 = (1 << 7), // cockpit monitor
    Monitor5 = (1 << 8), // cockpit monitor
    Monitor6 = (1 << 9), // cockpit monitor
    Monitor7 = (1 << 10), // cockpit monitor
    Monitor8 = (1 << 11), // cockpit monitor
    Facing = (1 << 12), // This subobject always faces the camera
    Viewer = (1 << 13), // This subobject is marked as a 'viewer'.
    Layer = (1 << 14), // This subobject is marked as part of possible secondary model rendering.
    WeaponBat = (1 << 15), // This subobject is part of a weapon battery
    Glow = (1 << 16), // This subobject glows
    Custom = (1 << 17), // This subobject has textures/colors that are customizable
    Thruster = (1 << 18), // This is a thruster subobject
    Jitter = (1 << 19), // This object jitters by itself
    Headlight = (1 << 20), // This suboject is a headlight
    Additive = 1 << 21, // NEW: Submodel contains additive textures
    Alpha = 1 << 22 // NEW: Submodel contains alpha textures
};

struct ModelFace {
    struct Vertex {
        short index;
        Vector2 uv;
    };

    List<Vertex> vertices;

    Color color = { 1, 1, 1 };
    short texNum = -1;

    Vector3 normal;
    Vector3 min, max;
};

struct Submodel {
    struct Vertex {
        Vector3 position;
        Vector3 normal;
        float alpha = 1;
    };

    // Originally keyframe data was stored in separate arrays on the submodel struct
    struct Keyframe {
        Vector3 axis; // the axis of rotation for each keyframe (keyframe_axis)
        float angle = 0; // The destination angles for each key frame (keyframe_angles)
        int frame = 0; // the frame position on the animation track
        Quaternion rotation = Quaternion::Identity;
    };

    List<Keyframe> keyframes;

    struct PositionKeyframe {
        int frame = 0;
        Vector3 position;    
    };

    List<PositionKeyframe> positionKeyframes;

    int numKeyAngles = 0;
    int rotTrackMin = 0, rotTrackMax = 0;
    int numKeyPos = 0;
    int posTrackMin = 0, posTrackMax = 0;

    Vector3 min, max;
    int parent;
    Vector3 normal; // Normal for separation plane
    Vector3 point; // Point for separation plane
    Vector3 offset; // Offset from parent
    float radius;

    int treeOffset, dataOffset;
    Vector3 geometricCenter;

    string name, props;
    int movementType, movementAxis;
    float rotation; // Fixed speed rotation along ? axis

    SubmodelFlag flags;

    List<Vertex> vertices;
    List<ModelFace> faces;

    Color glow;
    float glowSize;
};

struct Model {
    int version; // equals major * 100 + minor
    int majorVersion;
    float radius;
    Vector3 min, max;
    List<Submodel> submodels;
    List<string> textures;
    List<int> textureHandles;

    int frameMin = 0, frameMax = 0;

    ModelFlag flags;

    struct Bank {
        int parent = 0;
        Vector3 point, normal;
    };

    List<Bank> guns;
    List<Bank> groundPlanes;
    List<Bank> attachPoints;
    //List<bool> attachPointsUsed;

    struct WeaponBattery {
        // Static Data  (Add to robot generic page)
        List<ubyte> gunpoints;

        // Turrets are listed from most important (greatest mobility) to least important
        List<ushort> turrets;
    };

    List<WeaponBattery> weaponBatteries;

    static Model Read(StreamReader& r);
};
}
