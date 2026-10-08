#pragma pack_matrix(row_major)
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
    float percentLife : ANIMATION;
    float rotation : ROTATION;
};

// Common descriptors
ConstantBuffer<FrameConstants> Frame : register(b0, space1);
StructuredBuffer<TextureInfo> TextureInfoTable : register(t0, space1); // Texture / material information
Texture2DArray TextureTable[] : register(t1, space1);

StructuredBuffer<int> TextureHandles : register(t0); // mapping to texture table
StructuredBuffer<SpriteVertex> Vertices : register(t1);
Texture2D Depth : register(t2);

SamplerState Sampler : register(s0);


struct VS_OUT {
    float4 position : SV_Position;
    float2 uv : TEXCOORD0;
    centroid float4 color : COLOR0;
    centroid float3 world : TEXCOORD1;
    nointerpolation uint instance : TEXCOORD2;
    float radius : RADIUS;
    float percentLife : PERCENT_LIFE;
};

static const float2 BillboardOffsets[4] = { float2(-1, -1), float2(1, -1), float2(-1, 1), float2(1, 1) };

VS_OUT vsmain(uint id : SV_VertexID, uint instance: SV_InstanceID) {
    SpriteVertex vertex = Vertices[instance];
    float2 offset = BillboardOffsets[id % 4];
    float4 viewPos = mul(float4(vertex.position, 1), Frame.View);
    float bias = max(vertex.size.x, vertex.size.y) * 0.75;

    // Rotate sprite
    float2 vertexOffset = offset;

    if (vertex.rotation != 0) {
        float cosTheta = cos(vertex.rotation);
        float sinTheta = sin(vertex.rotation);
        vertexOffset = float2(offset.x * cosTheta - offset.y * sinTheta, offset.x * sinTheta + offset.y * cosTheta);
    }

    viewPos.xy += vertexOffset * vertex.size; // expand in screen-aligned plane

    VS_OUT output;
    output.position = mul(viewPos, Frame.Projection);
    // bias the billboard. todo: only apply when flagged
    output.position.z *= output.position.w / (output.position.w - bias);

    output.color = vertex.color;
    output.uv = offset * 0.5 + 0.5;
    output.instance = instance;
    output.radius = max(vertex.size.x, vertex.size.y);
    output.percentLife = vertex.percentLife;
    return output;
}

float SaturateSoft(float fade, float contrast) {
    float output = 0.5 * pow(saturate(2 * ((fade > 0.5) ? 1 - fade : fade)), contrast);
    return (fade > 0.5) ? 1 - output : output;
}

float4 psmain(VS_OUT pixel, uint primitiveID : SV_PrimitiveID) : SV_TARGET {
    //return float4(1, 1, 1, 1);
    uint textureIndex = TextureHandles[NonUniformResourceIndex(pixel.instance)];
    TextureInfo info = TextureInfoTable[NonUniformResourceIndex(textureIndex)];
    // return float4((float) info.frames, (float) info.index, (float) info.frameTime, 1);

    // todo: uv scroll from texture info
    float2 uv = pixel.uv + float2(0, 0) * Frame.Time;
    float4 color = float4(1, 1, 1, 1);

    if (info.frames > 1) {
        Texture2DArray tex = TextureTable[NonUniformResourceIndex(info.index)];

        float time = Frame.Time;
        bool clamp = pixel.percentLife >= 0;

        // fit the animation to match the particle lifespan
        if (pixel.percentLife >= 0) {
            float totalDuration = info.frameTime * info.frames;
            time = pixel.percentLife * totalDuration;
        }

        color = BlendTextureFrames(info, tex, Sampler, time, uv, 0, clamp);
    }
    else {
        Texture2DArray tex = TextureTable[NonUniformResourceIndex(info.index)];
        color = tex.Sample(Sampler, float3(uv, 0));
    }

    // Soft particles
    float sceneDepth = Depth.Sample(Sampler, pixel.position.xy / Frame.Size).r;
    float pixelDepth = Frame.NearClip / pixel.position.z;

    if (sceneDepth != 0) {
        const float DEPTH_EXPONENT = 2;
        const float fadeDistance = 1 * pixel.radius * .75;

        float depthDelta = sceneDepth - pixelDepth;
        // float fade = saturate(depthDelta / fadeDistance);
        float fade = SaturateSoft(depthDelta / fadeDistance, DEPTH_EXPONENT);
        color.a *= fade;
    }

    color.rgb *= pixel.color.rgb;
    color.a *= pixel.color.a;
    color.rgb = pow(color.rgb, 1 / 2.2);
    return color;
}
