#version 330 core

in vec2 TexCoord;                     // Interpolated texture coordinates
out vec4 FragColor;                   // Output fragment color

uniform sampler2D texture1;           // Sonar texture (e.g., a radar image)
uniform vec3 baseColor;               // Base color for sonar
uniform float time;                   // Animation time for pulsing effect

void main() {
    // Sample the texture and calculate a pulsing alpha based on time
    vec4 texColor = texture(texture1, TexCoord);
    float pulse = abs(sin(time));     // Create a pulsing effect
    FragColor = vec4(baseColor * pulse, texColor.a); // Combine base color with pulsing effect
}
