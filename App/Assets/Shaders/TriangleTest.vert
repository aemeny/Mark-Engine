#version 460
#extension GL_EXT_nonuniform_qualifier : enable

struct VertexData
{
    float px, py, pz;   // position
    float cx, cy, cz;   // colour
    float nx, ny, nz;   // normal
    float u,  v;        // uv
};



layout (binding = 0) readonly buffer Vertices { 
    VertexData data[]; 
} in_Vertices[];

layout (binding = 1) readonly buffer Indices { 
    uint data[]; 
} in_Indices[];

layout (binding = 2) uniform UniformBuffer { 
    mat4 WVP; 
} ubo;

layout (location = 0) out vec2 out_TexCoord;
layout (location = 1) flat out uint out_MeshIndex;

void main()
{
    uint meshIndex = nonuniformEXT(uint(gl_InstanceIndex));
    uint vertexIndex = in_Indices[meshIndex].data[gl_VertexIndex];
    VertexData vertex = in_Vertices[meshIndex].data[vertexIndex];

    vec3 pos = vec3(vertex.px, vertex.py, vertex.pz);

    gl_Position = ubo.WVP * vec4(pos, 1.0);

    out_TexCoord = vec2(vertex.u, vertex.v);
    out_MeshIndex = uint(gl_InstanceIndex);
}