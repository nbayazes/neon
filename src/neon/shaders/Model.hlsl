//#include "Lighting.hlsli"
#include "Common.hlsli"
#include "ObjectVertex.hlsli"

struct Constants {
    float4x4 World;
    //float4 EmissiveLight; // for additive objects like lasers
    //float4 Ambient;
    //float4 PhaseColor;
    //int TexIdOverride;
    //float TimeOffset;
    //float PhaseAmount;
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


struct VS_OUT {
    float4 position : SV_Position;
    float2 uv : TEXCOORD0;
    centroid float4 color : COLOR0;
    centroid float3 normal : NORMAL;
    centroid float3 tangent : TANGENT;
    centroid float3 bitangent : BITANGENT;
    centroid float3 world : TEXCOORD1;
};

VS_OUT vsmain(ObjectVertex input) {
    float4x4 wvp = mul(Frame.ViewProj, Object.World);
    VS_OUT output;
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

float4 SampleTexture(TextureInfo info, float2 uv) {
    if (info.frames > 1) {
        // float blend = info.GetFrameBlend(Frame.Time);

        Texture2DArray tex = TextureTable[NonUniformResourceIndex(info.index)];
        // tex = TextureTable[3];

        // "strong blink" behavior, skips to the first frame and then smoothly transitions. Looks good on the antenna / switch animation.
        //float3 rgb0 = tex.Sample(Sampler, float3(input.uv, floor(blend))).rgb;
        //float3 rgb1 = tex.Sample(Sampler, float3(input.uv, ceil(blend))).rgb;
        //rgb = lerp(rgb0, rgb1, fmod(blend, 1));

        int f0 = info.GetFrame(Frame.Time);
        int f1 = info.GetFrame(Frame.Time + info.frameTime);

        float4 rgb0 = tex.Sample(Sampler, float3(uv, f0));
        float4 rgb1 = tex.Sample(Sampler, float3(uv, f1));

        float blend = fmod(Frame.Time / info.frameTime, 1);
        // todo: vclip blend mode
        //blend = InOutCubic(blend);
        return lerp(rgb0, rgb1, blend);

        // Texture2DArray tex = TextureTable[NonUniformResourceIndex(info.index)];
        // rgb = tex.Sample(Sampler, float3(input.uv, info.GetFrame(Frame.Time))).rgb;
    }
    else {
        Texture2DArray tex = TextureTable[NonUniformResourceIndex(info.index)];
        // tex = TextureTable[10];
        // Texture2D tex = TextureTable[NonUniformResourceIndex(textureIndex)];
        return tex.Sample(Sampler, float3(uv, 0));
    }
}

float4 psmain(VS_OUT input, uint primitiveID : SV_PrimitiveID) : SV_TARGET {
    uint textureIndex = TextureIndices[NonUniformResourceIndex(primitiveID)];
    TextureInfo info = TextureInfoTable[NonUniformResourceIndex(textureIndex)];
    // return float4((float) info.frames, (float) info.index, (float) info.frameTime, 1);
    //float4 color = float4(1, 1, 1, 1);

    // todo: uv scroll from texture info
    float2 uv = input.uv + float2(0, 0) * Frame.Time;

    float4 color = SampleTexture(info, input.uv);
    color.a *= info.opacity;

    //if (info.frames > 1) {
    //    // float blend = info.GetFrameBlend(Frame.Time);

    //    Texture2DArray tex = TextureTable[NonUniformResourceIndex(info.index)];
    //    // tex = TextureTable[3];

    //    // "strong blink" behavior, skips to the first frame and then smoothly transitions. Looks good on the antenna / switch animation.
    //    //float3 rgb0 = tex.Sample(Sampler, float3(input.uv, floor(blend))).rgb;
    //    //float3 rgb1 = tex.Sample(Sampler, float3(input.uv, ceil(blend))).rgb;
    //    //rgb = lerp(rgb0, rgb1, fmod(blend, 1));

    //    int f0 = info.GetFrame(Frame.Time);
    //    int f1 = info.GetFrame(Frame.Time + info.frameTime);

    //    float4 rgb0 = tex.Sample(Sampler, float3(uv, f0));
    //    float4 rgb1 = tex.Sample(Sampler, float3(uv, f1));

    //    float blend = fmod(Frame.Time / info.frameTime, 1);
    //    // todo: vclip blend mode
    //    //blend = InOutCubic(blend);
    //    color = lerp(rgb0, rgb1, blend);

    //    // Texture2DArray tex = TextureTable[NonUniformResourceIndex(info.index)];
    //    // rgb = tex.Sample(Sampler, float3(input.uv, info.GetFrame(Frame.Time))).rgb;
    //}
    //else {
    //    Texture2DArray tex = TextureTable[NonUniformResourceIndex(info.index)];
    //    // tex = TextureTable[10];
    //    // Texture2D tex = TextureTable[NonUniformResourceIndex(textureIndex)];
    //    color = tex.Sample(Sampler, float3(uv, 0));
    //}

    color.rgb = pow(color.rgb, 1 / 2.2);

    float3 l = normalize(float3(4, 1, 5));
    color.rgb *= 1 + dot(normalize(input.normal * 2), l) * 0.5;
    return color;
}
