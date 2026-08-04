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

ConstantBuffer<FrameConstants> Frame : register(b0);
ConstantBuffer<Constants> Object : register(b1);
StructuredBuffer<int> TextureIndices : register(t0); // mapping to texture table

// lighting constants are register b2

SamplerState Sampler : register(s0);
SamplerState NormalSampler : register(s1);
//Texture2D Matcap : register(t0);
//StructuredBuffer<MaterialInfo> Materials : register(t5);
//StructuredBuffer<VClip> VClips : register(t6);
//Texture2D DissolveTexture : register(t7);
//TextureCube Environment : register(t8);

// primary texture table
Texture2D TextureTable[] : register(t0, space1);

struct PS_INPUT {
    float4 position : SV_Position;
    float2 uv : TEXCOORD0;
    centroid float4 color : COLOR0;
    centroid float3 normal : NORMAL;
    centroid float3 tangent : TANGENT;
    centroid float3 bitangent : BITANGENT;
    centroid float3 world : TEXCOORD1;
};

//[RootSignature(RS)]
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
    //output.texid = input.texid;
    return output;
}


float4 psmain() : SV_TARGET {
    return float4(1.0f, 1.0f, 1.0f, 1.0f);
}
