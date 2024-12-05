#version 330 core
out vec4 FragColor;

in vec2 TexCoord;
uniform sampler2D texture1;
uniform vec3 sonarColor;
uniform float time;

void main() {
    float pulse = 0.5 + 0.5 * sin(time * 3.0);
    vec3 animatedColor = sonarColor * pulse;
    FragColor = vec4(animatedColor, 1.0) * texture(texture1, TexCoord);
}
