#version 460

struct VertexData
{
    float px, py, pz;   // position
    float cx, cy, cz;   // colour
    float nx, ny, nz;   // normal
    float u,  v;        // uv
};

layout (binding = 0) readonly buffer Vertices { 
    VertexData data[]; 
} in_Vertices;

layout (binding = 1) readonly buffer Indices { 
    uint data[]; 
} in_Indices;

layout (binding = 2) /*readonly*/ uniform UniformBuffer { 
    mat4 WVP; 
} ubo;

layout (location = 0) out vec2 out_TexCoord;

void main()
{
    uint vertexIndex = in_Indices.data[gl_VertexIndex];
    VertexData vertex = in_Vertices.data[vertexIndex];

    vec3 pos = vec3(vertex.px, vertex.py, vertex.pz);

    gl_Position = ubo.WVP * vec4(pos, 1.0);

    out_TexCoord = vec2(vertex.u, vertex.v);
}