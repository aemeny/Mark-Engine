#version 460

struct VertexData
{
    float x, y, z;
    float u, v;
};

layout (binding = 0) readonly buffer Vertices { 
    VertexData data[]; 
} in_Vertices;

layout (binding = 1) /*readonly*/ uniform UniformBuffer { 
    mat4 WVP; 
} ubo;

layout (location = 0) out vec2 out_TexCoord;

void main()
{
    VertexData vertex = in_Vertices.data[gl_VertexIndex];

    vec3 pos = vec3(vertex.x, vertex.y, vertex.z);

    gl_Position = ubo.WVP * vec4(pos, 1.0);

    out_TexCoord = vec2(vertex.u, vertex.v);
}