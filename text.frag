// text.frag
#version 330 core
in vec2 TexCoords;
out vec4 FragColor;

uniform sampler2D uTexture;
uniform vec3 uColor;

void main() {
    float alpha = texture(uTexture, vec2(TexCoords.x, 1.0 - TexCoords.y)).r;
    FragColor = vec4(uColor, alpha);
}

