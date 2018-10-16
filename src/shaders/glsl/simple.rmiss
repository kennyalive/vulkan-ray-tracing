#version 460
#extension GL_NVX_raytracing : require

layout (location=0) rayPayloadInNVX vec3 color;

float srgb_encode(float c) {
    if (c <= 0.0031308f)
        return 12.92f * c;
    else
        return 1.055f * pow(c, 1.f/2.4f) - 0.055f;
}

void main() {
    color = vec3(srgb_encode(0.32f), srgb_encode(0.32f), srgb_encode(0.4f));
}
