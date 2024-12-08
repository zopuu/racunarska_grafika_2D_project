#version 330 core

layout (location = 0) in vec3 aPos;    // Position
layout (location = 1) in vec2 aTexCoord; // Texture coordinates

out vec2 TexCoord;

uniform mat4 uProjection;
uniform mat4 uModel;

void main()
{
    gl_Position = uProjection * uModel * vec4(aPos, 1.0);
    TexCoord = aTexCoord;
}
