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

ConstantBuffer<FrameConstants> Frame : register(b0, space1);
StructuredBuffer<TextureInfo> TextureInfoTable : register(t0, space1); // Texture / material information
Texture2DArray TextureTable[] : register(t1, space1);

ConstantBuffer<Constants> Instance : register(b0);
StructuredBuffer<int> TextureIndices : register(t0); // mapping to texture table


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
    float4x4 wvp = mul(Frame.ViewProj, Instance.World);
    VS_OUT output;
    output.position = mul(wvp, float4(input.position, 1));
    output.color = input.color;
    output.uv = input.uv;

    // transform from object space to world space
    output.normal = normalize(mul((float3x3)Instance.World, input.normal));
    output.tangent = normalize(mul((float3x3)Instance.World, input.tangent));
    output.bitangent = normalize(mul((float3x3)Instance.World, input.bitangent));
    output.world = mul(Instance.World, float4(input.position, 1)).xyz;

    return output;
}

float4 psmain(VS_OUT input, uint primitiveID : SV_PrimitiveID) : SV_TARGET {
    uint textureIndex = TextureIndices[NonUniformResourceIndex(primitiveID)];
    TextureInfo info = TextureInfoTable[NonUniformResourceIndex(textureIndex)];
    // return float4((float) info.frames, (float) info.index, (float) info.frameTime, 1);
    //float4 color = float4(1, 1, 1, 1);

    // todo: uv scroll from texture info
    float2 uv = input.uv + float2(0, 0) * Frame.Time;

    float4 color = float4(1, 1, 1, 1);

    if (info.frames > 1) {
        Texture2DArray tex = TextureTable[NonUniformResourceIndex(info.index)];
        color = BlendTextureFrames(info, tex, Sampler, Frame.Time, uv, 0);
    }
    else {
        Texture2DArray tex = TextureTable[NonUniformResourceIndex(info.index)];
        color = tex.Sample(Sampler, float3(uv, 0));
    }

    color.a *= info.opacity;
    color.rgb = pow(color.rgb, 1 / 2.2);

    float3 l = normalize(float3(4, 1, 5));
    color.rgb *= 1 + dot(normalize(input.normal * 2), l) * 0.5;
    return color;
}
