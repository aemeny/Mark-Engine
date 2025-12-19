#version 460

layout (location = 0) in vec2 in_uv;

layout (location = 0) out vec4 out_fragColor;

layout (binding = 3) uniform sampler2D texSampler;

void main()
{
    out_fragColor = texture(texSampler, in_uv);
}