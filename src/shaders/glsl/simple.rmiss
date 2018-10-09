#version 460
#extension GL_NVX_raytracing : require

layout (location=0) rayPayloadInNVX vec3 color;

void main() {
    color = vec3(0.19f, 0.04f, 0.14f);
}
