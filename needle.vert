#version 330 core

layout(location = 0) in vec2 aPos;   // Vertex position
layout(location = 1) in vec4 aColor; // Vertex color

out vec4 vColor; // Output to the fragment shader

uniform float uAspectRatio;   // Aspect ratio
uniform float uRotationAngle; // Rotation angle in radians

void main() {
    // Apply aspect ratio correction
    vec2 scaledPos = vec2(aPos.x / uAspectRatio, aPos.y);

    // Apply rotation transformation
    float cosAngle = cos(uRotationAngle);
    float sinAngle = sin(uRotationAngle);
    mat2 rotation = mat2(
        cosAngle, -sinAngle,
        sinAngle,  cosAngle
    );
    vec2 rotatedPos = rotation * scaledPos;

    // Set the final position and pass the color
    gl_Position = vec4(rotatedPos, 0.0, 1.0);
    vColor = aColor;
}
