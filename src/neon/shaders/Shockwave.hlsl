#pragma pack_matrix(row_major)

//#include "Lighting.hlsli"
#include "Common.hlsli"

struct Constants {
    float4x4 transform;
    int Texture;
};

ConstantBuffer<FrameConstants> Frame : register(b0, space1);
StructuredBuffer<TextureInfo> TextureInfoTable : register(t0, space1); // Texture / material information
Texture2DArray TextureTable[] : register(t1, space1);

ConstantBuffer<Constants> Instance : register(b0);


// lighting constants are register b2

SamplerState Sampler : register(s0);
SamplerState NormalSampler : register(s1);
//Texture2D Matcap : register(t0);
//StructuredBuffer<MaterialInfo> Materials : register(t5);
//StructuredBuffer<VClip> VClips : register(t6);
//Texture2D DissolveTexture : register(t7);
//TextureCube Environment : register(t8);

struct ShockwaveVertex {
    float3 position : POSITION;
    float2 uv : TEXCOORD0;
    float4 color : COLOR0;
};

struct VS_OUT {
    float4 position : SV_Position;
    float2 uv : TEXCOORD0;
    centroid float4 color : COLOR0;
};

VS_OUT vsmain(ShockwaveVertex input) {
    float4x4 wvp = mul(Instance.transform, Frame.ViewProj);

    //float distortAmount = 0.5;
    //float3 position = JitterVertex(input.position, int(Frame.Time * 21), distortAmount);

    VS_OUT output;
    //output.position = mul(float4(position, 1), wvp);
    output.position = mul(float4(input.position, 1), wvp);
    output.color = input.color;
    output.uv = input.uv;

    return output;
}

float4 psmain(VS_OUT pixel) : SV_TARGET {
    // todo: if texture is negative only using flat shading
    // return float4((float) info.frames, (float) info.index, (float) info.frameTime, 1);
    //float4 color = float4(1, 1, 1, 1);

    float2 uv = pixel.uv + float2(0, 0) * Frame.Time;
    //float4 color = float4(1, 1, 1, pixel.color.a);
    float4 color = float4(1, 1, 1, 1);

    if (Instance.Texture >= 0) {
        TextureInfo info = TextureInfoTable[NonUniformResourceIndex(Instance.Texture)];

        if (info.frames > 1) {
            Texture2DArray tex = TextureTable[NonUniformResourceIndex(info.index)];
            color *= BlendTextureFrames(info, tex, Sampler, Frame.Time, uv, 0);
        }
        else {
            Texture2DArray tex = TextureTable[NonUniformResourceIndex(info.index)];
            color *= tex.Sample(Sampler, float3(uv, 0));
        }

        color *= info.opacity;
    }

    if (color.a <= 0.001)
        discard;

    //color.a *= pow(info.opacity, 2.2);
    //color *= pow(info.opacity, 2.2);
    //color.a *= (sin(Frame.Time) + 1) * 0.5;
    color.rgb = pow(color.rgb, 1 / 2.2);

    //float3 l = normalize(float3(4, 1, 5));
    //color.rgb *= 1 + dot(normalize(pixel.normal * 2), l) * 0.5;
    return color;
}
