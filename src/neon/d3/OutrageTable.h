#pragma once

#include "neon-strings.h"
#include "neon-types.h"
#include "Streams.h"

namespace neon {

enum class ObjectType : uint8 {
    None = 255, // unused object
    SecretExitReturn = 254, // Editor only secret exit return. Not serialized.
    Wall = 0, // Not actually an object. Used for collisions
    Fireball = 1, // Explosion effect. no collision?
    Robot = 2,
    Hostage = 3,
    Player = 4,
    Weapon = 5, // A projectile from a weapon?
    Camera = 6,
    Powerup = 7,
    Debris = 8, // remains of a destroyed robot
    Reactor = 9,
    Clutter = 11, // Unused, would be for random clutter placed in the level like barrels or boxes
    Ghost = 12, // Dead player / spectator
    Light = 13, // Unused
    Coop = 14, // Co-op player
    Marker = 15, // A marker placed by the player
    Building = 16, // D3
    Door = 17 // D3
};

}

namespace neon::d3 {

constexpr int MAX_OBJ_SOUNDS = 2;
constexpr int MAX_AI_SOUNDS = 5;
constexpr int NUM_MOVEMENT_CLASSES = 5;
constexpr int NUM_ANIMS_PER_CLASS = 24;
constexpr int MAX_WBS_PER_OBJ = 21;
constexpr int MAX_WB_FIRING_MASKS = 8;
constexpr int MAX_WB_GUNPOINTS = 8;
constexpr int MAX_WB_UPGRADES = 5;
constexpr int MAX_DSPEW_TYPES = 2;

enum class TextureFlag {
    None = 0,
    Volatile = 1,
    Water = 1 << 1,
    Metal = 1 << 2, // Editor sorting
    Marble = 1 << 3, // Editor sorting
    Plastic = 1 << 4, // Editor sorting
    Forcefield = 1 << 5,
    Animated = 1 << 6,
    Destroyable = 1 << 7,
    Effect = 1 << 8,
    HudCockpit = 1 << 9,
    Mine = 1 << 10,
    Terrain = 1 << 11,
    Object = 1 << 12,
    Texture64 = 1 << 13,
    Tmap2 = 1 << 14,
    Texture32 = 1 << 15,
    FlyThru = 1 << 16,
    PassThru = 1 << 17,
    PingPong = 1 << 18,
    Light = 1 << 19, // Full bright
    Breakable = 1 << 20,
    Saturate = 1 << 21, // Additive
    Alpha = 1 << 22, // Use the alpha value in the tablefile
    DontUse = 1 << 23, // Not intended for levels? Hidden in texture browser?
    Procedural = 1 << 24,
    WaterProcedural = 1 << 25,
    ForceLightmap = 1 << 26,
    SaturateLightmap = 1 << 27,
    Texture256 = 1 << 28,
    Lava = 1 << 29,
    Rubble = 1 << 30,
    SmoothSpecular = 1 << 31
};

enum class FireProceduralType : uint8 {
    None,
    LineLightning,
    SphereLightning,
    Straight,
    RisingEmbers,
    RandomEmbers,
    Spinners,
    Roamers,
    Fountain,
    Cone,
    FallRight,
    FallLeft
};

enum class WaterProceduralType : uint8 {
    None,
    HeightBlob,
    SineBlob,
    RandomRaindrops,
    RandomBlobdrops,
    Line
};

// Procedural texture info
struct ProceduralInfo {
    float evalTime = 1 / 30.0f; // Delay in seconds between updates

    // (Fire) Palette encodes 255 colors in RGBA5551 format
    uint16 palette[255]{};
    uint8 heat = 0; // (Fire) Higher heat causes slower decay 

    // (Water) Lighting strength applied. Valid range is 0-31.
    // 0 will disable lighting and use a simpler distortion method. Otherwise lower values give a stronger strength of lighting.
    uint8 light = 0;

    // (Water) Thickness of the fluid. Valid range is 0-31.
    // Higher thickness values will cause ripples to decay slower. Lower values will cause them to decay faster.
    uint8 thickness = 0;
    float oscillateTime = 0; // (Water) Oscillates the value of thickness over time when not 0
    uint8 oscillateValue = 0; // (Water) Oscillates the value of thickness over time

    bool IsWater = false; // Copied from parent texture info for convenience

    struct Element {
        union {
            int8 type; // Determine type based on flags in TextureInfo
            WaterProceduralType waterType;
            FireProceduralType fireType;
        };

        int16 speed; // How quickly certain effects animate. Actually a uint8 but this is reused for water calcs which are negative.
        uint8 frequency; // Frames to wait between creating effect
        uint8 size;
        uint8 x1, y1; // Center point of effect
        uint8 x2, y2; // Only used by the end point of line lightning
    };

    List<Element> elements;
};

struct TextureInfo {
    string name; // Entry in tablefile
    string fileName; // File name in hog or on disk
    Color color;
    Vector2 slide;
    float speed; // Total time of animation. Used by vclips on non-explosions.
    float reflectivity; // For radiosity calcs 
    TextureFlag flags;
    int8 corona;
    int damage;
    ProceduralInfo procedural;
    string sound;

    uint16 GetSize() const {
        if (HasFlag(flags, d3::TextureFlag::Texture32))
            return 32u;
        else if (HasFlag(flags, d3::TextureFlag::Texture64))
            return 64u;
        else if (HasFlag(flags, d3::TextureFlag::Texture256))
            return 256u;

        return 128u;
    }
};

struct SoundInfo {
    string name; // Entry in tablefile
    string fileName; // File name in hog or on disk
    int flags;
    int loopStart, loopEnd;
    float outerConeVolume;
    int innerConeAngle, outerConeAngle;
    float minDistance, maxDistance;
    float importVolume;
};

struct AnimElem {
    int16 from;
    int16 to;
    float speed;
};

struct AnimClasses {
    std::array<AnimElem, NUM_ANIMS_PER_CLASS> elems;
};

struct PhysicsInfo {
    Vector3 velocity;
    Vector3 rotVel;
    int numBounces;
    float coeffRestitution;
    float mass;
    float drag;
    float rotDrag;
    float fullThrust;
    float fullRotThrust;
    float maxTurnrollRate;
    float turnrollRatio;
    float wiggleAmplitude;
    float wigglesPerSec;
    float hitDieDot;
    uint flags;
};

struct LightInfo {
    int flags;
    float lightDistance;
    Color color1;
    Color color2;
    float timeInterval;
    float flickerDistance;
    float directionalDot;
    int timeBits;
    ubyte angle;
    ubyte lightingRenderType;
};

enum class AINotifyFlag : uint32 {
    NewMovement = 1 << 1,
    ObjKilled = 1 << 2,
    WhitByObj = 1 << 3,
    SeeTarget = 1 << 4,
    PlayerSeesYou = 1 << 5,
    WhitObject = 1 << 6,
    TargetDied = 1 << 7,
    ObjFired = 1 << 8,
    GoalComplete = 1 << 9,
    GoalFail = 1 << 10,
    GoalError = 1 << 11,
    HearNoise = 1 << 12,
    NearTarget = 1 << 13,
    HitByWeapon = 1 << 14,
    NearWall = 1 << 15,
    UserDefined = 1 << 16,
    TargetInvalid = 1 << 17,
    GoalInvalid = 1 << 18,
    ScriptedGoal = 1 << 19,
    ScriptedEnabler = 1 << 20,
    AnimComplete = 1 << 21,
    BumpedObj = 1 << 22,
    MeleeHit = 1 << 23,
    MeleeAttackFrame = 1 << 24,
    ScriptedInfluence = 1 << 25,
    ScriptedOrient = 1 << 26,
    MovieStart = 1 << 27,
    MovieEnd = 1 << 28,
    FiredWeapon = 1 << 29,

    AlwaysOn = AnimComplete | NewMovement | PlayerSeesYou | GoalComplete | GoalFail | GoalError |
    UserDefined | TargetDied | TargetInvalid | BumpedObj | MeleeHit | MeleeAttackFrame
};

enum class AIFlag : uint32 {
    Weapon1 = 1 << 0,
    Weapon2 = 1 << 1,
    Melee1 = 1 << 2,
    Melee2 = 1 << 3,
    StaysInout = 1 << 4,
    ActAsNeutralUntilShot = 1 << 5,
    Persistant = 1 << 6,
    Dodge = 1 << 7,
    Fire = 1 << 8,
    Flinch = 1 << 9,
    DetermineTarget = 1 << 10,
    Aim = 1 << 11,
    OnlyTauntAtDeath = 1 << 12,
    AvoidWalls = 1 << 13,
    Disabled = 1 << 14,
    FluctuateSpeedProperties = 1 << 15,
    TeamMask1 = 1 << 16,
    TeamMask2 = 1 << 17,
    OrderedWBFiring = 1 << 18,
    OrientToVel = 1 << 19,
    XZDist = 1 << 20,
    ReportNewOrient = 1 << 21,
    TargetByDist = 1 << 22,
    DisableFiring = 1 << 23,
    DisableMelee = 1 << 24,
    AutoAvoidFriends = 1 << 25,
    TrackClosest2Friends = 1 << 26,
    TrackClosest2Enemies = 1 << 27,
    BiasedFlightHeight = 1 << 28,
    ForceAwareness = 1 << 29,
    UVecFov = 1 << 30,
    AimPntFov = 1u << 31,

    TeamMask = TeamMask1 | TeamMask2,
};

struct AIInfo {
    ubyte aiClass;
    ubyte aiType;

    float maxVelocity;
    float maxDeltaVelocity;
    float maxTurnRate;
    float maxDeltaTurnRate;

    float attackVelPercent;
    float fleeVelPercent;
    float dodgeVelPercent;

    float circleDistance;
    float dodgePercent;

    Array<float, 2> meleeDamage;
    Array<float, 2> meleeLatency;

    Array<int, MAX_AI_SOUNDS> sound;

    ubyte movementType;
    ubyte movementSubtype;

    AIFlag flags;
    AINotifyFlag notifyFlags;

    float fov;

    float avoidFriendsDistance;

    float frustration;
    float curiosity;
    float lifePreservation;
    float aggression;

    float fireSpread;
    float nightVision;
    float fogVision;
    float leadAccuracy;
    float leadVariance;
    float fightTeam;
    float fightSame;
    float hearing;
    float roaming;

    float biasedFlightImportance;
    float biasedFlightMin;
    float biasedFlightMax;
};

struct AnimInfo {
    Array<AnimClasses, NUM_MOVEMENT_CLASSES> classes;
};

struct WeaponBatteryInfo {
    Array<uint16, MAX_WB_GUNPOINTS> gpWeaponIndex;
    Array<uint16, MAX_WB_FIRING_MASKS> fmFireSoundIndex;
    uint16 aimingGPIndex;

    ubyte numMasks;
    Array<ubyte, MAX_WB_FIRING_MASKS> gpFireMasks;
    Array<float, MAX_WB_FIRING_MASKS> gpFireWait;

    ubyte gpQuadFireMask;

    ubyte numLevels;
    Array<uint16, MAX_WB_UPGRADES> gpLevelWeaponIndex;
    Array<uint16, MAX_WB_UPGRADES> gpLevelFireSoundIndex;

    ubyte aimingFlags;
    float aiming3DDot; // These Can be Reused.
    float aiming3DDist;
    float aimingXZDot;

    Array<float, MAX_WB_FIRING_MASKS> animStartFrame;
    Array<float, MAX_WB_FIRING_MASKS> animFireFrame;
    Array<float, MAX_WB_FIRING_MASKS> animEndFrame;
    Array<float, MAX_WB_FIRING_MASKS> animTime;

    uint16 flags;

    float energyUsage;
    float ammoUsage;
};

struct DeathInfo {
    int flags;
    float delayMin;
    float delayMax;
    ubyte probabilities;
};

enum class GenericFlag {
    None = 0,
    ControlAI = 1 << 0,
    UsesPhysics = 1 << 1,
    Destroyable = 1 << 2,
    InvenSelectable = 1 << 3,
    InvenNonuseable = 1 << 4,
    InvenTypeMission = 1 << 5,
    InvenNoremove = 1 << 6,
    InvenViswhenused = 1 << 7,
    AIScriptedDeath = 1 << 8,
    DoCeilingCheck = 1 << 9, // Check terrain 'ceiling' collision
    IgnoreForcefieldsAndGlass = 1 << 10,
    NoDiffScaleDamage = 1 << 11,
    NoDiffScaleMove = 1 << 12,
    AmbientObject = 1 << 13
};

struct GenericInfo {
    ObjectType type{};
    string name;
    string modelName;
    string medModelName;
    string loModelName;
    float impactSize{};
    float impactTime{};
    float damage{};
    int score{};
    int ammoCount{};
    string moduleName;
    string scriptNameOverride;
    string description;
    string iconName;
    float medLodDistance{};
    float loLodDistance{};
    PhysicsInfo physics{};
    float size{};
    LightInfo light{};
    int hitPoints{};
    GenericFlag flags{};
    AIInfo ai{};
    ubyte dspewFlags{};
    Array<float, MAX_DSPEW_TYPES> dspewPercent{};
    Array<int16, MAX_DSPEW_TYPES> dspewNumber{};
    Array<string, MAX_DSPEW_TYPES> dspewGenericNames;
    AnimInfo anim;
    Array<WeaponBatteryInfo, MAX_WBS_PER_OBJ> weaponBatteries;
    Array<Array<string, MAX_WB_GUNPOINTS>, MAX_WBS_PER_OBJ> wbWeaponNames;
    Array<string, MAX_OBJ_SOUNDS> soundNames;
    Array<string, MAX_AI_SOUNDS> aiSoundNames;
    Array<Array<string, MAX_WB_GUNPOINTS>, MAX_WBS_PER_OBJ> wbSoundNames;
    Array<Array<string, NUM_ANIMS_PER_CLASS>, NUM_MOVEMENT_CLASSES> animSoundNames;
    float respawnScalar{};
    List<DeathInfo> deathTypes;
};

// Descent 3 Game Table (GAM). Contains metadata for game assets.
struct GameTable {
    enum {
        TABLE_FILE_BASE = 0,
        TABLE_FILE_MISSION = 1,
        TABLE_FILE_MODULE = 2
    } type{};

    string name;

    List<TextureInfo> textures;
    List<SoundInfo> sounds;
    List<GenericInfo> objects;

    static GameTable Read(StreamReader&);

    TextureInfo* FindTexture(string_view texture) {
        for (auto& tex : textures) {
            if (String::EqualsIgnoreCase(tex.name, texture))
                return &tex;
        }

        return nullptr;
    }
};

}
