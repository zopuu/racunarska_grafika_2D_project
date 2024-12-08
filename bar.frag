#version 330 core

in vec2 TexCoord;                     // Interpolated texture coordinates
out vec4 FragColor;                   // Output fragment color

uniform vec3 barColor;                // Color of the bar
uniform sampler2D texture1;           // Optional texture (if needed for gradient effects)
uniform float fillAmount;             // Percentage fill of the bar (0.0 to 1.0)

void main() {
    // Map the texture coordinates to the bar's fill amount
    if (TexCoord.y > fillAmount) {
        discard;                      // Discard fragments above the fill level
    }
    vec4 texColor = texture(texture1, TexCoord);
    FragColor = vec4(barColor, 1.0) * texColor; // Combine bar color with texture
}
