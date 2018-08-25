[[vk::binding(0, 0)]]
cbuffer Constants {
    float4x4 mvp;
};

[[vk::binding(1, 0)]]
Texture2D texture;

[[vk::binding(2, 0)]]
SamplerState texture_sampler;

struct VS2PS {
    float4 position : SV_POSITION;
    float2 uv : TEXCOORD;
};

VS2PS main_vs(
    float4 position : POSITION,
    float2 uv      : TEXCOORD,
    float3 normal : NORMAL)
{
    VS2PS data;
    data.position = mul(mvp, position);
    data.uv = uv;
    return data;
}

float4 main_fs(VS2PS data) : SV_TARGET
{
    return texture.Sample(texture_sampler, data.uv);
}
