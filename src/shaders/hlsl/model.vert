[[vk::binding(0, 0)]]
cbuffer Constants {
    float4x4 mvp;
};

struct VS2PS {
    float4 position : SV_POSITION;
    float2 uv : TEXCOORD;
};

VS2PS main(
    float4 position : POSITION,
    float2 uv      : TEXCOORD,
    float3 normal : NORMAL)
{
    VS2PS data;
    data.position = mul(mvp, position);
    data.uv = uv;
    return data;
}
