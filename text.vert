// text.vert
#version 330 core
layout (location = 0) in vec4 vertex; 
// vertex.xy = position, vertex.zw = texcoords
out vec2 TexCoords;

uniform mat4 uProjection;

void main() {
    gl_Position = uProjection * vec4(vertex.xy, 0.0, 1.0);
    TexCoords = vertex.zw;
}
