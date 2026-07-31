#include "pch.h"
#include "OutrageModel.h"
#include <fmt/format.h>
#include <spdlog/spdlog.h>
#include "neon-strings.h"
#include "Streams.h"
#include "Utility.h"

namespace neon::d3 {

constexpr auto MAX_MODEL_TEXTURES = 35;

string ReadModelString(StreamReader& r) {
    int mlen = r.ReadInt32();
    return r.ReadString(mlen);
}

// Gets the real center of a polygon and total area
std::pair<Vector3, float> GetCentroid(span<Vector3> src) {
    if (src.size() < 3) return { Vector3::Zero, 1.0f };
    // First figure out the total area of this polygon
    auto normal = (src[1] - src[0]).Cross(src[2] - src[0]);
    auto totalArea = normal.Length() / 2;

    for (int i = 2; i < src.size() - 1; i++) {
        auto n = (src[i] - src[0]).Cross(src[i + 1] - src[0]);
        totalArea += n.Length() / 2;
    }

    // Now figure out how much weight each triangle represents 
    // to the overall polygon
    normal = (src[1] - src[0]).Cross(src[2] - src[0]);
    auto area = normal.Length() / 2; // copy of initial?

    // Get the center of the first polygon
    Vector3 center;
    for (int i = 0; i < 3; i++)
        center += src[i];

    center /= 3;

    Vector3 centroid = center * (area / totalArea);

    // Now do the same for the rest	
    for (int i = 2; i < src.size() - 1; i++) {
        normal = (src[i] - src[0]).Cross(src[i + 1] - src[0]);
        area = normal.Length() / 2;

        center += src[0] + src[i] + src[i + 1];
        center /= 3;

        centroid += center * (area / totalArea);
    }

    return { centroid, totalArea };
}

void ParseSubmodelProperties(Submodel& sm) {
    const auto& props = sm.props;
    const auto len = props.length();

    if (len < 3)
        return;

    auto i = Seq::indexOf(sm.props, '=').value_or(len);

    auto command = String::Trim(String::ToLower(props.substr(0, i + 1)));
    auto data = i == len ? "" : String::Trim(props.substr(i + 1));

    switch (String::Hash(command)) {
        case String::Hash("$rotate="): {
            auto spinRate = std::stof(data);
            if (spinRate <= 0 || spinRate > 20) {
                SPDLOG_WARN("Rotate command with invalid spin rate of {}. Must be between 0 and 20", spinRate);
                return; // bad data
            }

            SetFlag(sm.flags, SubmodelFlag::Rotate);
            sm.rotation = 1.0f / spinRate;
            return;
        }

        case String::Hash("$jitter"):
            SetFlag(sm.flags, SubmodelFlag::Jitter);
            return;

        case String::Hash("$shell"):
            SetFlag(sm.flags, SubmodelFlag::Shell);
            return;

        case String::Hash("$facing"):
            SetFlag(sm.flags, SubmodelFlag::Facing);
            return;

        case String::Hash("$frontface"):
            SetFlag(sm.flags, SubmodelFlag::Frontface);
            return;

        case String::Hash("$thruster="):
        case String::Hash("$glow="): {
            auto split = String::Split(data, ',');
            if (split.size() != 4) {
                SPDLOG_WARN("Read a glow or thruster command with invalid parameters `{}`. Must have 4 comma separated values", data);
                return; // warn, invalid
            }

            bool isGlow = String::Hash("$glow=") == String::Hash(command);
            SetFlag(sm.flags, isGlow ? SubmodelFlag::Glow : SubmodelFlag::Thruster);
            sm.glow.x = std::stof(split[0]);
            sm.glow.y = std::stof(split[1]);
            sm.glow.z = std::stof(split[2]);
            sm.glowSize = std::stof(split[3]);
            return;
        }

        case String::Hash("$fov="): {
            // todo: FOV data
            return;
        }
        // monitors

        case String::Hash("$viewer"):
            SetFlag(sm.flags, SubmodelFlag::Viewer);
            return;

        case String::Hash("$layer"):
            SetFlag(sm.flags, SubmodelFlag::Layer);
            return;

        case String::Hash("$custom"):
            SetFlag(sm.flags, SubmodelFlag::Custom);
            return;
    }
}

void UpdateMinMax(Submodel& sm) {
    Vector3 min = { 90000, 90000, 90000 };
    Vector3 max = { -90000, -90000, -90000 };

    for (auto& v : sm.vertices) {
        min = Vector3::Min(min, v.position);
        max = Vector3::Max(max, v.position);
    }

    sm.min = min;
    sm.max = max;
}

void Postprocess(Submodel& sm) {
    // build angle matrices

    // check if parent equals self

    if (sm.numKeyAngles == 0 && HasFlag(sm.flags, SubmodelFlag::Rotate)) {
        
        SPDLOG_WARN("Submodel is rotator without keyframe");
        ClearFlag(sm.flags, SubmodelFlag::Rotate);
    }

    if (sm.numKeyAngles == 0 && HasFlag(sm.flags, SubmodelFlag::Turret)) {
        SPDLOG_WARN("Submodel is turret without keyframe");
        ClearFlag(sm.flags, SubmodelFlag::Turret);
    }

    if (HasFlag(sm.flags, SubmodelFlag::Facing)) {
        List<Vector3> verts = Seq::map(sm.vertices, [](const auto& v) { return v.position; });
        auto [centroid, area] = GetCentroid(verts);
        sm.radius = sqrt(area) / 2;
    };

    // todo: decode animations
}

Model Model::Read(StreamReader& r) {
    // can also load data from oof, but let's assume POFs
    auto fileId = r.ReadInt32();
    if (fileId != 'OPSP')
        throw Exception("Not a model file");

    Model pm{};
    pm.version = r.ReadInt32();

    if (pm.version < 18)
        pm.version *= 100; // fix old version

    if (pm.version < MIN_OBJFILE_VERSION || pm.version > OBJFILE_VERSION)
        throw Exception("Bad version");

    pm.majorVersion = pm.version / 100;

    if (pm.majorVersion >= 21)
        pm.flags |= ModelFlag::LightmapRes;
        SetFlag(pm.flags, ModelFlag::LightmapRes);

    bool timed = false;
    if (pm.majorVersion >= 22) {
        timed = true;
        SetFlag(pm.flags, ModelFlag::Timed);
    }

    while (!r.EndOfStream()) {
        auto id = r.ReadInt32();
        auto len = r.ReadInt32();
        auto chunkStart = r.Position();
        if (len <= 0) throw Exception("bad chunk length");

        switch (id) {
            case MakeFourCC("OHDR"): // POF file header
            {
                auto submodels = r.ReadInt32Checked(1000, "bad submodel count");
                pm.submodels.reserve(submodels);
                pm.radius = r.ReadFloat();
                pm.min = r.ReadVector3();
                pm.max = r.ReadVector3();

                int detail = r.ReadInt32();
                for (int i = 0; i < detail; i++)
                    r.ReadInt32(); // Skip details

                break;
            }

            case MakeFourCC("TXTR"): // Texture filename list
            {
                auto count = r.ReadInt32Checked(MAX_MODEL_TEXTURES, "exceeded max model textures");
                pm.textures.resize(count);

                for (auto& tex : pm.textures)
                    tex = ReadModelString(r)/* + ".ogf"*/;

                break;
            }

            case MakeFourCC("SOBJ"): // Subobject header
            {
                auto& sm = pm.submodels.emplace_back();

                r.ReadInt32Checked((int)pm.submodels.size(), "too many submodels");
                sm.parent = r.ReadInt32();
                sm.normal = r.ReadVector3();

                /*auto d =*/
                r.ReadFloat();
                sm.point = r.ReadVector3();
                sm.offset = r.ReadVector3();
                sm.radius = r.ReadFloat();

                sm.treeOffset = r.ReadInt32();
                sm.dataOffset = r.ReadInt32();

                if (pm.version > 1805)
                    sm.geometricCenter = r.ReadVector3();

                sm.name = ReadModelString(r);
                sm.props = ReadModelString(r);

                try {
                    ParseSubmodelProperties(sm);
                }
                catch (const std::exception&) {
                    throw Exception(fmt::format("Error parsing submodel props: {}", sm.props));
                }

                sm.movementType = r.ReadInt32();
                sm.movementAxis = r.ReadInt32();

                // skip freespace chunks
                auto chunks = r.ReadInt32();
                for (int i = 0; i < chunks; i++)
                    r.ReadInt32();

                sm.vertices.resize(r.ReadInt32Checked(2500, "too many verts"));

                for (auto& vert : sm.vertices)
                    vert.position = r.ReadVector3();

                for (auto& vert : sm.vertices)
                    vert.normal = r.ReadVector3();

                if (pm.majorVersion >= 23) {
                    for (auto& vert : sm.vertices) {
                        vert.alpha = r.ReadFloat();
                        if (vert.alpha < 0.99f)
                            SetFlag(pm.flags, ModelFlag::Alpha);
                    }
                }

                sm.faces.resize(r.ReadInt32Checked(20000, "too many faces"));

                for (auto& face : sm.faces) {
                    face.normal = r.ReadVector3();
                    face.vertices.resize(r.ReadInt32Checked(100, "bad nverts"));

                    bool textured = (bool)r.ReadInt32();
                    if (textured)
                        face.texNum = (short)r.ReadInt32();
                    else
                        face.color = r.ReadRGB();

                    for (auto& v : face.vertices) {
                        v.index = (short)r.ReadInt32();
                        v.uv.x = r.ReadFloat();
                        v.uv.y = r.ReadFloat();
                    }

                    // Lightmap stuff we don't care about
                    if (pm.majorVersion >= 21) {
                        /*auto xdiff =*/
                        r.ReadFloat();
                        /*auto ydiff =*/
                        r.ReadFloat();
                    }
                }

                break;
            }

            case MakeFourCC("GPNT"): // gun points
            {
                pm.guns.resize(r.ReadInt32Checked(100, "bad number of guns"));

                for (auto& gun : pm.guns) {
                    // In Version 19.08 and beyond, gunpoints are associated to their parent object.
                    if (pm.version >= 19 * 100 + 8)
                        gun.parent = r.ReadInt32();

                    gun.point = r.ReadVector3();
                    gun.normal = r.ReadVector3();
                }
                break;
            }

            case MakeFourCC("WBAT"): // weapon batteries
            {
                auto num = r.ReadInt32Checked(100, "bad number of weapon batteries");
                pm.weaponBatteries.resize(num);

                for (auto& battery : pm.weaponBatteries) {
                    auto gunpoints = r.ReadInt32Checked(100, "bad number of weapon battery gunpoints");
                    battery.gunpoints.resize(gunpoints);
                    for (auto& gp : battery.gunpoints)
                        gp = (int8)r.ReadInt32();

                    auto turrets = r.ReadInt32Checked(100, "bad turret num");
                    battery.turrets.resize(turrets);
                    for (auto& turret : battery.turrets)
                        turret = (ushort)r.ReadInt32();
                }

                break;
            }

            case MakeFourCC("PANI"): // positional animation data
            {
                int nframes = timed ? 0 : r.ReadInt32();

                for (auto& sm : pm.submodels) {
                    if (timed) {
                        sm.numKeyPos = r.ReadInt32();
                        sm.posTrackMin = r.ReadInt32();
                        sm.posTrackMax = r.ReadInt32();

                        // clamp
                        if (sm.posTrackMin < pm.frameMin)
                            pm.frameMin = sm.posTrackMin;

                        if (sm.posTrackMax < pm.frameMax)
                            pm.frameMax = sm.posTrackMax;

                        //int numTicks = sm.PosTrackMax - sm.PosTrackMin;

                        // lookup
                        //if (numTicks > 0)
                        //    sm.TickPosRemap.resize(numTicks * 2);
                    }
                    else {
                        sm.numKeyPos = nframes;
                    }

                    for (auto& key : sm.keyframes) {
                        if (timed)
                            key.posStartTime = r.ReadInt32();

                        key.position = r.ReadVector3();
                    }
                }

                break;
            }

            case MakeFourCC("RANI"): // rotational animation data
            case MakeFourCC("ANIM"): // animation data
            {
                int nframes = 0;

                if (!timed) {
                    nframes = r.ReadInt32();
                    // pm.num key angles = nframes
                }

                for (auto& sm : pm.submodels) {
                    if (timed) {
                        sm.numKeyAngles = r.ReadInt32();
                        sm.rotTrackMin = r.ReadInt32();
                        sm.rotTrackMax = r.ReadInt32();

                        if (sm.rotTrackMin < pm.frameMin)
                            pm.frameMin = sm.rotTrackMin;

                        if (sm.rotTrackMax > pm.frameMax)
                            pm.frameMax = sm.rotTrackMax;
                    }
                    else {
                        sm.numKeyAngles = nframes;
                    }

                    if (sm.numKeyAngles > 10000)
                        throw Exception("Bad number of key angles");

                    sm.keyframes.resize(sm.numKeyAngles /*+ 1*/); // why the +1?

                    if (timed) {
                        //int numTicks = sm.RotTrackMax - sm.RotTrackMin;

                        // Some kind of lookup...
                        //if (numTicks > 0) {
                        //    sm.TickAngleRemap.resize(numTicks * 2);
                        //}
                    }

                    for (auto& keyframe : sm.keyframes) {
                        if (timed)
                            keyframe.rotStartTime = r.ReadInt32();

                        keyframe.axis = r.ReadVector3();
                        keyframe.axis.Normalize();
                        keyframe.angle = r.ReadInt32();

                        // some stuff here about keyframe angle wrapping?
                    }
                }

                break;
            }

            case MakeFourCC("GRND"): // ground plane info
            {
                auto slots = r.ReadInt32Checked(100, "bad ground plane count");
                pm.groundPlanes.resize(slots);
                for (auto& plane : pm.groundPlanes) {
                    plane.parent = r.ReadInt32();
                    plane.point = r.ReadVector3();
                    plane.normal = r.ReadVector3();
                }
                break;
            }

            case MakeFourCC("ATCH"): // attach points
            {
                auto points = r.ReadInt32Checked(100, "Bad number of attach points");
                pm.attachPoints.resize(points);
                //pm.attachPointsUsed.resize(points);

                for (auto& point : pm.attachPoints) {
                    point.parent = r.ReadInt32();
                    point.point = r.ReadVector3();
                    point.normal = r.ReadVector3();
                }

                break;
            }

            case MakeFourCC("NATH"): // attach normals
            {
                auto normalCount = r.ReadInt32();
                if (pm.attachPoints.size() != normalCount)
                    throw Exception("Invalid ATTACH normals - total number doesn't match number of attach points");

                for (int i = 0; i < normalCount; i++) {
                    pm.attachPoints[i].point = r.ReadVector3();
                    pm.attachPoints[i].normal = r.ReadVector3();
                    //pm.attachPointsUsed[i] = true;
                }

                break;
            }

            case MakeFourCC("PINF"): // POF file information, like command line, etc
            {
                //List<ubyte> buffer(len);
                //r.ReadBytes(buffer);
                break;
            }

            case MakeFourCC("SPCL"): // Only contains dummy data
            {
                //List<ubyte> buffer(len);
                //r.ReadBytes(buffer);
                break;
            }

            default:
                SPDLOG_WARN("unknown chunk id {}", id);
                break;
        }

        r.Seek(chunkStart + len); // seek to next chunk (prevents read errors due to individual chunks)
    }

    for (auto& submodel : pm.submodels) {
        UpdateMinMax(submodel);
        Postprocess(submodel);
    }

    return pm;
}
}
