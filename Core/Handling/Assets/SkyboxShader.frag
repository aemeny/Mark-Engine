#version 460

layout (location = 0) in vec3 Dir;

layout (location = 0) out vec4 out_Colour;

layout (set = 0, binding = 1) uniform samplerCube skybox;

void main()
{
    out_Colour = texture(skybox, normalize(Dir));
}