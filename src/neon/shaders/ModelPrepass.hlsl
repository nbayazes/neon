#include "Common.hlsli"
#include "ObjectVertex.hlsli"

//struct ObjectArgs {
//    float4x4 world;
//    float dissolve;
//    float timeOffset;
//};

struct InstanceConstants {
    float4x4 World;
};

ConstantBuffer<FrameConstants> Frame : register(b0, space1);
StructuredBuffer<TextureInfo> TextureInfoTable : register(t0, space1); // Texture / material information
Texture2DArray TextureTable[] : register(t1, space1);

ConstantBuffer<InstanceConstants> Instance : register(b0);
StructuredBuffer<int> TextureHandles : register(t0); // mapping to texture table

struct PS_INPUT {
    centroid float4 position : SV_Position;
    float2 uv : TEXCOORD0;
    float depth : TEXCOORD1;
};

PS_INPUT vsmain(ObjectVertex input) {
    float4x4 wvp = mul(Frame.ViewProj, Instance.World);
    PS_INPUT output;
    output.position = mul(wvp, float4(input.position, 1));
    output.uv = input.uv;
    output.depth = output.position.w; // W before perspective divide
    return output;
}

float psmain(PS_INPUT pixel) : SV_Target {
    //if (Object.dissolve > 0)
    //{
    //    float dissolveTex = 1 - Sample2D(DissolveTexture, input.uv + float2(Object.timeOffset, Object.timeOffset), Sampler, Frame.FilterMode).r;
    //    clip(Object.dissolve - dissolveTex);
    //}

    //int texid = input.texid;
    //if (texid > VCLIP_RANGE)
    //{
    //    texid = VClips[texid - VCLIP_RANGE].GetFrame(Frame.Time + Object.TimeOffset);
    //}

    //float alpha = Sample2D(TextureTable[texid * 5], input.uv, Sampler, Frame.FilterMode).a;

    // Use <= 0 to use cutout edge AA, but it introduces artifacts. < 1 causes aliasing.
    //if (alpha < 1)
    //    discard;

    //return LinearizeDepth(Frame.NearClip, pixel.depth);
    return pixel.depth;
}
