#version 330 core

out vec4 FragColor;

in vec2 TexCoord;

uniform sampler2D uTexture;
uniform vec4 uColor; // For coloring elements without a texture
uniform float uUseTexture; // 1.0 if using texture, 0.0 if solid color

void main()
{
    if(uUseTexture == 1.0) {
        FragColor = texture(uTexture, TexCoord);
    } else {
        FragColor = uColor;
    }
}
