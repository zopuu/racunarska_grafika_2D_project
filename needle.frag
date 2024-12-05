#version 330 core

in vec4 vColor;       // Interpolated color from the vertex shader
out vec4 FragColor;   // Final color output

void main() {
    FragColor = vColor; // Simply output the interpolated color
}
