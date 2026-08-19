#include "Common.hlsli"

struct Constants {
    float4 FogColor;
    float DepthBias;
    float Softness;
    int FilterMode;
};

struct SpriteVertex {
    float3 position : POSITION;
    float4 color : COLOR0;
    float2 size : SIZE;
};

// Common descriptors
ConstantBuffer<FrameConstants> Frame : register(b0);
StructuredBuffer<TextureInfo> TextureInfoTable : register(t0); // Texture / material information
Texture2DArray TextureTable[] : register(t0, space1);

StructuredBuffer<int> TextureHandles : register(t1); // mapping to texture table
StructuredBuffer<SpriteVertex> Vertices : register(t2);

SamplerState Sampler : register(s0);


struct VS_OUT {
    float4 position : SV_Position;
    float2 uv : TEXCOORD0;
    centroid float4 color : COLOR0;
    centroid float3 world : TEXCOORD1;
    nointerpolation uint instance : TEXCOORD2;
};

static const float2 BillboardOffsets[4] = { float2(-1, -1), float2(1, -1), float2(-1, 1), float2(1, 1) };

VS_OUT vsmain(uint id : SV_VertexID, uint instance: SV_InstanceID) {
    SpriteVertex vertex = Vertices[instance];
    float2 offset = BillboardOffsets[id % 4];
    // float4 viewPos = mul(float4(vertex.position, 1), Frame.ViewProj);
    float4 viewPos = mul(Frame.View, float4(vertex.position, 1));
    // float4 viewPos = mul(Frame.View, float4(vertex.position, 1));
    viewPos.xy += offset * vertex.size; // expand in screen-aligned plane
    viewPos.z -= max(vertex.size.x, vertex.size.y); // bias the billboard. todo: only apply when flagged
    VS_OUT output;
    output.position = viewPos;
    // output.position = mul(viewPos, Frame.Projection);
    output.position = mul(Frame.Projection, viewPos);
    output.color = vertex.color;
    output.uv = offset * 0.5 + 0.5;
    output.instance = instance;
    return output;
}

float4 psmain(VS_OUT input, uint primitiveID : SV_PrimitiveID) : SV_TARGET {
    //return float4(1, 1, 1, 1);
    uint textureIndex = TextureHandles[NonUniformResourceIndex(input.instance)];
    TextureInfo info = TextureInfoTable[NonUniformResourceIndex(textureIndex)];
    // return float4((float) info.frames, (float) info.index, (float) info.frameTime, 1);

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

    //color.a *= info.opacity;

    color.rgb = pow(color.rgb, 1 / 2.2);
    return color;
}
