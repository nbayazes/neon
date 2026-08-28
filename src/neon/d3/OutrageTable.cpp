#include "pch.h"
#include "OutrageTable.h"
#include "neon-types.h"

namespace neon::d3 {

constexpr auto PAGENAME_LEN = 35;

enum PageType {
    PAGETYPE_TEXTURE = 1,
    PAGETYPE_DOOR = 5,
    PAGETYPE_SOUND = 7,
    PAGETYPE_GENERIC = 10,
};

constexpr int MAX_STRING_LEN = 256;
constexpr int MAX_MODULENAME_LEN = 32;
constexpr int MAX_DESCRIPTION_LEN = 1024;

SoundInfo ReadSoundPage(StreamReader& r) {
    constexpr int KNOWN_VERSION = 1;
    auto version = r.ReadInt16();
    if (version > KNOWN_VERSION)
        throw Exception("Unsupported texture info version");

    SoundInfo si{};
    si.name = r.ReadCString(PAGENAME_LEN);
    si.fileName = r.ReadCString(PAGENAME_LEN);
    si.flags = r.ReadInt32();
    si.loopStart = r.ReadInt32();
    si.loopEnd = r.ReadInt32();
    si.outerConeVolume = r.ReadFloat();
    si.innerConeAngle = r.ReadInt32();
    si.outerConeAngle = r.ReadInt32();
    si.maxDistance = r.ReadFloat();
    si.minDistance = r.ReadFloat();
    si.importVolume = r.ReadFloat();
    return si;
}

TextureInfo ReadTexturePage(StreamReader& r) {
    constexpr int KNOWN_VERSION = 7;
    auto version = r.ReadInt16();
    if (version > KNOWN_VERSION)
        throw Exception("Unsupported texture info version");

    TextureInfo tex{};
    tex.name = r.ReadCString(MAX_STRING_LEN);
    tex.fileName = r.ReadCString(MAX_STRING_LEN);
    std::ignore = r.ReadCString(MAX_STRING_LEN);

    tex.color.x = r.ReadFloat();
    tex.color.y = r.ReadFloat();
    tex.color.z = r.ReadFloat();
    tex.color.w = r.ReadFloat();

    tex.speed = r.ReadFloat();
    tex.slide.x = r.ReadFloat();
    tex.slide.y = r.ReadFloat();
    tex.reflectivity = r.ReadFloat();

    tex.corona = r.ReadByte();
    tex.damage = r.ReadInt32();

    tex.flags = (TextureFlag)r.ReadInt32();

    if (HasFlag(tex.flags, TextureFlag::Procedural)) {
        auto& proc = tex.procedural;
        for (auto& p : tex.procedural.palette)
            p = r.ReadUInt16();

        proc.IsWater = HasFlag(tex.flags, TextureFlag::WaterProcedural);
        proc.heat = r.ReadByte();
        proc.light = r.ReadByte();
        proc.thickness = r.ReadByte();
        proc.evalTime = r.ReadFloat();

        if (proc.evalTime <= 0.001f)
            proc.evalTime = 1 / 30.0f; // Default to 30 FPS if eval time is near 0

        if (version >= 6) {
            proc.oscillateTime = r.ReadFloat();
            proc.oscillateValue = r.ReadByte();
        }

        int n = r.ReadInt16(); // elements
        if (n < 0 || n > 1024)
            throw Exception("Procedural elements out of range");

        proc.elements.resize(n);

        for (auto& e : proc.elements) {
            e.type = r.ReadByte();
            e.frequency = r.ReadByte();
            e.speed = r.ReadByte();
            e.size = r.ReadByte();
            e.x1 = r.ReadByte();
            e.y1 = r.ReadByte();
            e.x2 = r.ReadByte();
            e.y2 = r.ReadByte();
        }
    }

    if (version >= 5) {
        if (version < 7)
            r.ReadInt16();
        else
            tex.sound = r.ReadCString(MAX_STRING_LEN);
        r.ReadFloat();
    }
    return tex;
}

PhysicsInfo ReadPhysicsInfo(StreamReader& r) {
    PhysicsInfo phys{};
    phys.mass = r.ReadFloat();
    phys.drag = r.ReadFloat();
    phys.fullThrust = r.ReadFloat();
    phys.flags = r.ReadInt32();
    phys.rotDrag = r.ReadFloat();
    phys.fullRotThrust = r.ReadFloat();
    phys.numBounces = r.ReadInt32();
    phys.velocity.z = r.ReadFloat();
    phys.rotVel = r.ReadVector3();
    phys.wiggleAmplitude = r.ReadFloat();
    phys.wigglesPerSec = r.ReadFloat();
    phys.coeffRestitution = r.ReadFloat();
    phys.hitDieDot = r.ReadFloat();
    phys.maxTurnrollRate = r.ReadFloat();
    phys.turnrollRatio = r.ReadFloat();
    return phys;
}

LightInfo ReadLightInfo(StreamReader& r) {
    LightInfo light{};
    light.lightDistance = r.ReadFloat();
    light.color1 = Color(r.ReadVector3());
    light.timeInterval = r.ReadFloat();
    light.flickerDistance = r.ReadFloat();
    light.directionalDot = r.ReadFloat();
    light.color2 = Color(r.ReadVector3());
    light.flags = r.ReadInt32();
    light.timeBits = r.ReadInt32();
    light.angle = r.ReadByte();
    light.lightingRenderType = r.ReadByte();
    return light;
}

AIInfo ReadAIInfo(StreamReader& r, int version, GenericFlag genFlags) {
    AIInfo ai{};
    ai.flags = (AIFlag)r.ReadInt32();
    ai.aiClass = r.ReadByte();
    ai.aiType = r.ReadByte();
    ai.movementType = r.ReadByte();
    ai.movementSubtype = r.ReadByte();
    ai.fov = r.ReadFloat();
    ai.maxVelocity = r.ReadFloat();
    ai.maxDeltaVelocity = r.ReadFloat();
    ai.maxTurnRate = r.ReadFloat();
    ai.notifyFlags = (AINotifyFlag)r.ReadInt32() | AINotifyFlag::AlwaysOn;
    ai.maxDeltaTurnRate = r.ReadFloat();
    ai.circleDistance = r.ReadFloat();
    ai.attackVelPercent = r.ReadFloat();
    ai.dodgePercent = r.ReadFloat();
    ai.dodgeVelPercent = r.ReadFloat();
    ai.fleeVelPercent = r.ReadFloat();
    ai.meleeDamage[0] = r.ReadFloat();
    ai.meleeDamage[1] = r.ReadFloat();
    ai.meleeLatency[0] = r.ReadFloat();
    ai.meleeLatency[1] = r.ReadFloat();
    ai.curiosity = r.ReadFloat();
    ai.nightVision = r.ReadFloat();
    ai.fogVision = r.ReadFloat();
    ai.leadAccuracy = r.ReadFloat();
    ai.leadVariance = r.ReadFloat();
    ai.fireSpread = r.ReadFloat();
    ai.fightTeam = r.ReadFloat();
    ai.fightSame = r.ReadFloat();
    ai.aggression = r.ReadFloat();
    ai.hearing = r.ReadFloat();
    ai.frustration = r.ReadFloat();
    ai.roaming = r.ReadFloat();
    ai.lifePreservation = r.ReadFloat();
    if (version < 16) {
        if ((bool)(genFlags & GenericFlag::UsesPhysics) && ai.maxVelocity > 0) {
            ai.flags |= AIFlag::AutoAvoidFriends;
            ai.avoidFriendsDistance = ai.circleDistance * 0.1f;
            if (ai.avoidFriendsDistance > 4.0f)
                ai.avoidFriendsDistance = 4.0f;
        }
        else
            ai.avoidFriendsDistance = 4.0f;
    }
    else
        ai.avoidFriendsDistance = r.ReadFloat();
    if (version < 17) {
        ai.biasedFlightImportance = 0.5f;
        ai.biasedFlightMin = 10.0f;
        ai.biasedFlightMax = 50.0f;
    }
    else {
        ai.biasedFlightImportance = r.ReadFloat();
        ai.biasedFlightMin = r.ReadFloat();
        ai.biasedFlightMax = r.ReadFloat();
    }
    return ai;
}

AnimInfo ReadAnimInfo(StreamReader& r, int version) {
    AnimInfo anim{};
    for (int i = 0; i < NUM_MOVEMENT_CLASSES; i++)
        for (int j = 0; j < NUM_ANIMS_PER_CLASS; j++) {
            auto& elem = anim.classes[i].elems[j];
            if (version < 20) {
                elem.from = r.ReadByte();
                elem.to = r.ReadByte();
            }
            else {
                elem.from = r.ReadInt16();
                elem.to = r.ReadInt16();
            }
            elem.speed = r.ReadFloat();
        }
    return anim;
}

DeathInfo ReadDeathInfo(StreamReader& r) {
    DeathInfo dt{};
    dt.flags = r.ReadInt32();
    dt.delayMin = r.ReadFloat();
    dt.delayMax = r.ReadFloat();
    dt.probabilities = r.ReadByte();
    return dt;
}

WeaponBatteryInfo ReadWeaponBatteryInfo(StreamReader& r, int version) {
    WeaponBatteryInfo wb{};

    wb.energyUsage = r.ReadFloat();
    wb.ammoUsage = r.ReadFloat();
    for (int i = 0; i < MAX_WB_GUNPOINTS; i++)
        wb.gpWeaponIndex[i] = r.ReadInt16();

    for (int i = 0; i < MAX_WB_FIRING_MASKS; i++) {
        wb.gpFireMasks[i] = r.ReadByte();
        wb.gpFireWait[i] = r.ReadFloat();
        wb.animTime[i] = r.ReadFloat();
        wb.animStartFrame[i] = r.ReadFloat();
        wb.animFireFrame[i] = r.ReadFloat();
        wb.animEndFrame[i] = r.ReadFloat();
    }
    wb.numMasks = r.ReadByte();
    wb.aimingGPIndex = r.ReadInt16();
    wb.aimingFlags = r.ReadByte();
    wb.aiming3DDot = r.ReadFloat();
    wb.aiming3DDist = r.ReadFloat();
    wb.aimingXZDot = r.ReadFloat();
    wb.flags = version < 2 ? r.ReadByte() : r.ReadInt16();
    wb.gpQuadFireMask = r.ReadByte();
    return wb;
}

void ReadGenericPage(StreamReader& r, GenericInfo& info) {
    constexpr int KNOWN_VERSION = 27;
    auto version = r.ReadInt16();
    if (version > KNOWN_VERSION)
        throw Exception("Unsupported generic info version");

    info.type = (ObjectType)r.ReadByte();
    info.name = r.ReadCString(PAGENAME_LEN);
    info.modelName = r.ReadCString(PAGENAME_LEN);
    info.medModelName = r.ReadCString(PAGENAME_LEN);
    info.loModelName = r.ReadCString(PAGENAME_LEN);
    info.impactSize = r.ReadFloat();
    info.impactTime = r.ReadFloat();
    info.damage = r.ReadFloat();
    info.score = version < 24 ? r.ReadByte() : r.ReadInt16();

    if (info.type == ObjectType::Powerup) {
        if (version < 25)
            info.ammoCount = 0;
        else
            info.ammoCount = r.ReadInt16();
    }
    else
        info.ammoCount = 0;

    std::ignore = r.ReadCString(MAX_STRING_LEN); // old script name

    if (version >= 18)
        info.moduleName = r.ReadCString(MAX_MODULENAME_LEN);
    if (version >= 19)
        info.scriptNameOverride = r.ReadCString(PAGENAME_LEN);
    if (r.ReadByte())
        info.description = r.ReadCString(MAX_DESCRIPTION_LEN);

    info.iconName = r.ReadCString(PAGENAME_LEN);
    info.medLodDistance = r.ReadFloat();
    info.loLodDistance = r.ReadFloat();

    info.physics = ReadPhysicsInfo(r);
    info.size = r.ReadFloat();
    info.light = ReadLightInfo(r);

    info.hitPoints = r.ReadInt32();
    info.flags = (GenericFlag)r.ReadInt32();
    info.ai = ReadAIInfo(r, version, info.flags);

    for (int i = 0; i < MAX_DSPEW_TYPES; i++) {
        info.dspewFlags = r.ReadByte();
        info.dspewPercent[i] = r.ReadFloat();
        info.dspewNumber[i] = r.ReadInt16();
        info.dspewGenericNames[i] = r.ReadCString(PAGENAME_LEN);
    }

    info.anim = ReadAnimInfo(r, version);

    for (int i = 0; i < MAX_WBS_PER_OBJ; i++)
        info.weaponBatteries[i] = ReadWeaponBatteryInfo(r, version);

    for (int i = 0; i < MAX_WBS_PER_OBJ; i++)
        for (int j = 0; j < MAX_WB_GUNPOINTS; j++)
            info.wbWeaponNames[i][j] = r.ReadCString(PAGENAME_LEN);

    for (int i = 0; i < MAX_OBJ_SOUNDS; i++)
        info.soundNames[i] = r.ReadCString(PAGENAME_LEN);

    if (version < 26)
        std::ignore = r.ReadCString(PAGENAME_LEN); // unused sound

    for (int i = 0; i < MAX_AI_SOUNDS; i++)
        info.aiSoundNames[i] = r.ReadCString(PAGENAME_LEN);

    for (int i = 0; i < MAX_WBS_PER_OBJ; i++)
        for (int j = 0; j < MAX_WB_FIRING_MASKS; j++)
            info.wbSoundNames[i][j] = r.ReadCString(PAGENAME_LEN);

    for (int i = 0; i < NUM_MOVEMENT_CLASSES; i++)
        for (int j = 0; j < NUM_ANIMS_PER_CLASS; j++)
            info.animSoundNames[i][j] = r.ReadCString(PAGENAME_LEN);

    info.respawnScalar = version >= 21 ? r.ReadFloat() : 1.0f;

    if (version >= 22) {
        int n = r.ReadInt16();
        for (int i = 0; i < n; i++)
            info.deathTypes.push_back(ReadDeathInfo(r));
    }

    if (version < 20 &&
        (info.type == ObjectType::Robot || info.type == ObjectType::Building) &&
        HasFlag(info.flags, GenericFlag::ControlAI) && HasFlag(info.flags, GenericFlag::Destroyable))
        info.score = info.hitPoints * 3;
}

GameTable GameTable::Read(StreamReader& r) {
    GameTable table{};

    while (!r.EndOfStream()) {
        auto pageType = r.ReadByte();
        auto pageStart = r.Position();
        auto len = r.ReadInt32();
        if (len <= 0) throw Exception("bad page length");

        switch (pageType) {
            case PAGETYPE_TEXTURE:
                table.textures.push_back(ReadTexturePage(r));
                break;

            case PAGETYPE_SOUND:
                table.sounds.push_back(ReadSoundPage(r));
                break;

            case PAGETYPE_GENERIC:
                // GenericInfo is quite large so emplace and pass by ref to prevent stack size warning
                ReadGenericPage(r, table.objects.emplace_back());
                break;
        }

        //auto readbytes = r.Position() - pageStart;
        r.Seek(pageStart + len); // seek to next chunk (prevents read errors due to individual chunks)
    }

    return table;
}

}
