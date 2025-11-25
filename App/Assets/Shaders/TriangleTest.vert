#version 460

struct VertexData
{
    float x, y, z;
    float u, v;
};

layout (binding = 0) readonly buffer Vertices { 
    VertexData data[]; 
} in_Vertices;

void main()
{
    VertexData vertex = in_Vertices.data[gl_VertexIndex];

    vec3 pos = vec3(vertex.x, vertex.y, vertex.z);

    gl_Position = vec4(pos, 1.0);
}