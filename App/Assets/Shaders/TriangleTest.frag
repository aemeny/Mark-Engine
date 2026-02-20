#version 460
#extension GL_EXT_nonuniform_qualifier : enable

layout (location = 0) in vec2 in_uv;
layout (location = 1) flat in uint in_MeshIndex;

layout (location = 0) out vec4 out_fragColor;

layout (binding = 3) uniform sampler2D texSampler[];

void main()
{
    uint textureIndex = nonuniformEXT(in_MeshIndex);
    out_fragColor = texture(texSampler[textureIndex], in_uv);
}