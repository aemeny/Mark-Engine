#version 460
#extension GL_EXT_nonuniform_qualifier : enable

layout (location = 0) in vec2 in_uv;

layout (location = 0) out vec4 out_fragColor;

layout(push_constant) uniform PushConstants
{
    uint meshIndex;    // Unsed in Fragment (kept for shared layout with Vertex Shader)
    uint textureIndex; // Which texture to sample
} pushConstant;

layout (binding = 3) uniform sampler2D texSampler[];

void main()
{
    uint textureIndex = nonuniformEXT(pushConstant.textureIndex);
    out_fragColor = texture(texSampler[textureIndex], in_uv);
}