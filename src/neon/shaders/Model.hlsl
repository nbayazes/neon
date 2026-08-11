//#include "Lighting.hlsli"
#include "Common.hlsli"
#include "ObjectVertex.hlsli"

//#define RS "RootFlags(ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT), "\
//    "CBV(b0),"\
//    "DescriptorTable(SRV(t0, space = 1, numDescriptors = unbounded, flags = DESCRIPTORS_VOLATILE), visibility=SHADER_VISIBILITY_PIXEL), " \
//    "CBV(b1), "\
//    "DescriptorTable(Sampler(s0), visibility=SHADER_VISIBILITY_PIXEL), "\
//    "DescriptorTable(Sampler(s1), visibility=SHADER_VISIBILITY_PIXEL)"

//#define RS "RootFlags(ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT), "\
//    "CBV(b0),"\
//    "DescriptorTable(SRV(t0, space = 1, numDescriptors = unbounded, flags = DESCRIPTORS_VOLATILE), visibility=SHADER_VISIBILITY_PIXEL), " \
//    "CBV(b1), "\
//    "DescriptorTable(Sampler(s0), visibility=SHADER_VISIBILITY_PIXEL), "\
//    "DescriptorTable(Sampler(s1), visibility=SHADER_VISIBILITY_PIXEL), "\
//    "DescriptorTable(SRV(t11), visibility=SHADER_VISIBILITY_PIXEL), " \
//    "DescriptorTable(SRV(t12), visibility=SHADER_VISIBILITY_PIXEL), " \
//    "DescriptorTable(SRV(t13), visibility=SHADER_VISIBILITY_PIXEL), " \
//    "CBV(b2)"

struct Constants {
    float4x4 World;
    //float4 EmissiveLight; // for additive objects like lasers
    //float4 Ambient;
    //float4 PhaseColor;
    //int TexIdOverride;
    //float TimeOffset;
    //float PhaseAmount;
};

struct TextureInfo {
    int index;
    int frames; // for animations
    float frameTime;
    int pingpong;

    int GetFrame(float time) {
        if (frames <= 1)
            return 0;

        int frame = (int)(abs(time) / frameTime);

        if (pingpong) {
            frame %= frames * 2;

            if (frame >= frames)
                frame = (frames - 1) - (frame % frames);
            else
                frame %= frames;

            return frame;
        }
        else {
            return frame % frames;
        }
    }

    float GetFrameBlend(float time) {
        if (frames <= 1)
            return 0;

        float frame = abs(time) / frameTime;

        if (pingpong) {
            float cycle = fmod(frame, frames * 2);

            if (cycle >= frames)
                cycle = (frames - 1) - fmod(cycle, frames);
            else
                cycle = fmod(cycle, frames);

            return cycle;
        }
        else {
// check if frame is past the end and wrap it
// for 5 frames, act like there's a 6th and return a negative value
            return fmod(frame, frames);
        }
    }
};

ConstantBuffer<FrameConstants> Frame : register(b0);
ConstantBuffer<Constants> Object : register(b1);
StructuredBuffer<int> TextureIndices : register(t0); // mapping to texture table
StructuredBuffer<TextureInfo> TextureInfoTable : register(t1); // Texture / material information

Texture2DArray TextureTable[] : register(t0, space1);

// lighting constants are register b2

SamplerState Sampler : register(s0);
SamplerState NormalSampler : register(s1);
//Texture2D Matcap : register(t0);
//StructuredBuffer<MaterialInfo> Materials : register(t5);
//StructuredBuffer<VClip> VClips : register(t6);
//Texture2D DissolveTexture : register(t7);
//TextureCube Environment : register(t8);


struct PS_INPUT {
    float4 position : SV_Position;
    float2 uv : TEXCOORD0;
    centroid float4 color : COLOR0;
    centroid float3 normal : NORMAL;
    centroid float3 tangent : TANGENT;
    centroid float3 bitangent : BITANGENT;
    centroid float3 world : TEXCOORD1;
};

PS_INPUT vsmain(ObjectVertex input) {
    float4x4 wvp = mul(Frame.ViewProj, Object.World);
    PS_INPUT output;
    output.position = mul(wvp, float4(input.position, 1));
    output.color = input.color;
    output.uv = input.uv;

    // transform from object space to world space
    output.normal = normalize(mul((float3x3)Object.World, input.normal));
    output.tangent = normalize(mul((float3x3)Object.World, input.tangent));
    output.bitangent = normalize(mul((float3x3)Object.World, input.bitangent));
    output.world = mul(Object.World, float4(input.position, 1)).xyz;

    return output;
}

float4 psmain(PS_INPUT input, uint primitiveID : SV_PrimitiveID) : SV_TARGET {
    uint textureIndex = TextureIndices[NonUniformResourceIndex(primitiveID)];
    TextureInfo info = TextureInfoTable[NonUniformResourceIndex(textureIndex)];
    // return float4((float) info.frames, (float) info.index, (float) info.frameTime, 1);
    float3 rgb = float3(1, 1, 1);

    if (info.frames > 1) {
        // float blend = info.GetFrameBlend(Frame.Time);

        Texture2DArray tex = TextureTable[NonUniformResourceIndex(info.index)];

        // "strong blink" behavior, skips to the first frame and then smoothly transitions. Looks good on the antenna / switch animation.
        //float3 rgb0 = tex.Sample(Sampler, float3(input.uv, floor(blend))).rgb;
        //float3 rgb1 = tex.Sample(Sampler, float3(input.uv, ceil(blend))).rgb;
        //rgb = lerp(rgb0, rgb1, fmod(blend, 1));

        int f0 = info.GetFrame(Frame.Time);
        // int f1 = (f0 + 1) % info.frames;
        int f1 = info.GetFrame(Frame.Time + info.frameTime);

        float3 rgb0 = tex.Sample(Sampler, float3(input.uv, f0)).rgb;
        float3 rgb1 = tex.Sample(Sampler, float3(input.uv, f1)).rgb;

        float blend = fmod(Frame.Time / info.frameTime, 1);

        rgb = lerp(rgb0, rgb1, blend);

        // Texture2DArray tex = TextureTable[NonUniformResourceIndex(info.index)];
        // rgb = tex.Sample(Sampler, float3(input.uv, info.GetFrame(Frame.Time))).rgb;
    }
    else {
        Texture2DArray tex = TextureTable[NonUniformResourceIndex(info.index)];
        // Texture2D tex = TextureTable[NonUniformResourceIndex(textureIndex)];
        rgb = tex.Sample(Sampler, float3(input.uv, 0)).rgb;
    }

    rgb = pow(rgb, 1 / 2.2);

    float3 l = normalize(float3(4, 1, 5));
    rgb *= 1 + dot(normalize(input.normal * 2), l) * 0.5;
    return float4(rgb, 1);
}
