[[vk::binding(1, 0)]]
Texture2D texture;

[[vk::binding(2, 0)]]
SamplerState texture_sampler;

struct PS_In {
    float4 position : SV_POSITION;
    float3 normal : NORMAL;
    float2 uv : TEXCOORD;
};

float srgb_encode(float c) {
    if (c <= 0.0031308f)
        return 12.92f * c;
    else
        return 1.055f * pow(c, 1.f/2.4f) - 0.055f;
}

float4 main(PS_In input) : SV_TARGET {
    // float2 uvdx = abs(ddx(input.uv));
    // float2 uvdy = abs(ddy(input.uv));
    // float filter_width = 2.0 * max(max(uvdx[0], uvdx[1]), max(uvdy[0], uvdy[1]));
    // float f = 150.0 * (filter_width - 0.0015);
    // return float4(srgb_encode(f), srgb_encode(f), srgb_encode(f), 1.0);

    float4 color = texture.Sample(texture_sampler, input.uv);
    return float4(srgb_encode(color.r), srgb_encode(color.g), srgb_encode(color.b), color.a);
}
