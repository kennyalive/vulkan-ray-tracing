[[vk::binding(0, 0)]]
cbuffer Constants {
    float4x4 model_view_proj;
    float4x4 model_view;
};

struct PS_In {
    float4 position : SV_POSITION;
    float3 normal : NORMAL;
    float2 uv : TEXCOORD;
};

PS_In main(
    float4 position : POSITION,
    float3 normal : NORMAL,
    float2 uv : TEXCOORD)
{
    PS_In output;
    output.position = mul(model_view_proj, position);
    output.normal = mul(model_view, float4(normal, 0)).xyz;
    output.uv = uv;
    return output;
}
