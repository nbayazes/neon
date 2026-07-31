#ifndef __OBJECT_VERTEX_H__
#define __OBJECT_VERTEX_H__

struct ObjectVertex {
    float3 position : POSITION;
    float2 uv : TEXCOORD0;
    float4 color : COLOR0;
    float3 normal : NORMAL;
    float3 tangent : TANGENT;
    float3 bitangent : BITANGENT;
};

#endif