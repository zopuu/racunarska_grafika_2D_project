#include <GL/glew.h>
#include <GLFW/glfw3.h>
#include <ft2build.h>
#include FT_FREETYPE_H
#include <iostream>
#include <sstream>
#include <vector>
#include <cstdlib>
#include <ctime>
#include <cmath>
#include <map>
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>
#define STB_IMAGE_IMPLEMENTATION
#include "stb_image.h"
#include <fstream>


// Function prototypes
void drawSonar(float deltaTime, int width, int height);
void drawNeedle(float aspectRatio);
void drawDots(float aspectRatio);
void handleInput(GLFWwindow* window, int width, int height, float deltaTime);
void drawButton(int width, int height);
void drawDepthIndicator(int width, int height);
void renderText(unsigned int shader, float x, float y, const std::string& text, float scale);
bool isMouseInButton(GLFWwindow* window, int width, int height);
unsigned int createShaderProgram(const char* vertexPath, const char* fragmentPath);


unsigned int textShaderID;

// Global variables for simulation
bool radarEnabled = true;          // Toggle for radar
float sonarTime = 0.0f;            // Tracks time for animations
float submarineDepth = 0.0f;       // Current submarine depth
float targetDepth = 0.0f;          // Target depth to interpolate to

float oxygenLevel = 100.0f; // Oxygen level in percentage (100% initially)
bool redLightOn = false;   // Status of the red warning light
bool greenLightOn = false; // Status of the green "safe" light
float oxygenDepletionRate = 5.0f; // Oxygen depletion rate per second


struct Character {
    unsigned int textureID; // Texture ID
    int sizeX, sizeY;       // Width and height of character
    int bearingX, bearingY; // Offset from baseline
    unsigned int advance;   // Advance to the next glyph
};
// Global variables for simulation// Tracks time for animations
struct RedDot {
    float x, y;
    float creationTime;
};
std::vector<RedDot> redDots;       // List of red dots
float lastDotSpawnTime = 0.0f;     // Time when the last red dot appeared// List to store red dots

std::map<char, Character> characters;
unsigned int textVAO, textVBO;
unsigned int backgroundVAO, backgroundVBO;
unsigned int backgroundShader;
unsigned int backgroundTexture;



// Initialize FreeType and load font
bool initializeFreeType(const std::string& fontPath) {
    FT_Library ft;
    if (FT_Init_FreeType(&ft)) {
        std::cerr << "Could not init FreeType Library" << std::endl;
        return false;
    }

    FT_Face face;
    if (FT_New_Face(ft, fontPath.c_str(), 0, &face)) {
        std::cerr << "Failed to load font: " << fontPath << std::endl;
        return false;
    }

    FT_Set_Pixel_Sizes(face, 0, 48); // Set font size
    glPixelStorei(GL_UNPACK_ALIGNMENT, 1);

    for (unsigned char c = 0; c < 128; c++) {
        if (FT_Load_Char(face, c, FT_LOAD_RENDER)) {
            std::cerr << "Failed to load Glyph: " << c << std::endl;
            continue;
        }

        unsigned int texture;
        glGenTextures(1, &texture);
        glBindTexture(GL_TEXTURE_2D, texture);
        glTexImage2D(
            GL_TEXTURE_2D,
            0,
            GL_RED,
            face->glyph->bitmap.width,
            face->glyph->bitmap.rows,
            0,
            GL_RED,
            GL_UNSIGNED_BYTE,
            face->glyph->bitmap.buffer
        );

        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);

        Character character = {
            texture,
            (int)face->glyph->bitmap.width,
            (int)face->glyph->bitmap.rows,
            (int)face->glyph->bitmap_left,
            (int)face->glyph->bitmap_top,
            (unsigned int)face->glyph->advance.x
        };
        characters.insert(std::pair<char, Character>(c, character));
    }

    glBindTexture(GL_TEXTURE_2D, 0);
    FT_Done_Face(face);
    FT_Done_FreeType(ft);

    return true;
}


void initializeTextRendering() {
    glGenVertexArrays(1, &textVAO);
    glGenBuffers(1, &textVBO);
    glBindVertexArray(textVAO);
    glBindBuffer(GL_ARRAY_BUFFER, textVBO);
    glBufferData(GL_ARRAY_BUFFER, sizeof(float) * 6 * 4, nullptr, GL_DYNAMIC_DRAW);
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(0, 4, GL_FLOAT, GL_FALSE, 4 * sizeof(float), 0);
    glBindBuffer(GL_ARRAY_BUFFER, 0);
    glBindVertexArray(0);
}


void renderText(unsigned int shader, float x, float y, const std::string& text, float scale) {
    glUseProgram(shader); // Use text shader
    // Remove the hardcoded text color setting here:
    // glUniform3f(glGetUniformLocation(shader, "textColor"), 1.0f, 1.0f, 1.0f);

    glActiveTexture(GL_TEXTURE0);
    glBindVertexArray(textVAO);

    for (char c : text) {
        if (characters.find(c) == characters.end()) continue;

        Character ch = characters[c];
        float xpos = x + ch.bearingX * scale;
        float ypos = y - (ch.sizeY - ch.bearingY) * scale;
        float w = ch.sizeX * scale;
        float h = ch.sizeY * scale;

        float vertices[6][4] = {
            {xpos, ypos + h, 0.0f, 0.0f},
            {xpos, ypos, 0.0f, 1.0f},
            {xpos + w, ypos, 1.0f, 1.0f},

            {xpos, ypos + h, 0.0f, 0.0f},
            {xpos + w, ypos, 1.0f, 1.0f},
            {xpos + w, ypos + h, 1.0f, 0.0f}
        };

        glBindTexture(GL_TEXTURE_2D, ch.textureID);
        glBindBuffer(GL_ARRAY_BUFFER, textVBO);
        glBufferSubData(GL_ARRAY_BUFFER, 0, sizeof(vertices), vertices);

        glDrawArrays(GL_TRIANGLES, 0, 6);
        x += (ch.advance >> 6) * scale;
    }

    // Reset bindings
    glBindVertexArray(0);
    glBindTexture(GL_TEXTURE_2D, 0);
    glUseProgram(0); // Unbind text shader
}





void handleInput(GLFWwindow* window, int width, int height, float deltaTime) {
    static bool buttonPressed = false;

    // Detect mouse click inside the button area
    if (glfwGetMouseButton(window, GLFW_MOUSE_BUTTON_LEFT) == GLFW_PRESS) {
        if (!buttonPressed && isMouseInButton(window, width, height)) {
            radarEnabled = !radarEnabled; // Toggle radar
            buttonPressed = true;
        }
    }
    else {
        buttonPressed = false; // Reset button press state
    }

    // Adjust target depth with W/S keys
    if (glfwGetKey(window, GLFW_KEY_W) == GLFW_PRESS) {
        targetDepth += 50.0f * deltaTime;
        if (targetDepth > 250.0f) targetDepth = 250.0f;
    }
    if (glfwGetKey(window, GLFW_KEY_S) == GLFW_PRESS) {
        targetDepth -= 50.0f * deltaTime;
        if (targetDepth < 0.0f) targetDepth = 0.0f;
    }

    // Smoothly interpolate submarine depth towards target depth
    if (std::abs(submarineDepth - targetDepth) > 0.1f) {
        submarineDepth += (targetDepth - submarineDepth) * deltaTime * 2.0f;
    }
    else {
        submarineDepth = targetDepth;
    }
}

void drawDepthIndicator(int width, int height) {
    float normalizedDepth = submarineDepth / 250.0f;

    float barWidth = 0.05f;
    float barHeight = 0.8f;
    float right = 0.8f;
    float bottom = -0.4f;

    // Draw gradient edges around the bar
    float edgeThickness = 0.015f; // Thickness of the gradient edges
    int gradientSteps = 10;      // Number of gradient steps for smoothness

    for (int i = 0; i < gradientSteps; ++i) {
        float t = i / (float)gradientSteps; // Normalize step (0.0 to 1.0)
        float outerOffset = edgeThickness * (1.0f - t); // Shrink outward to inward
        float innerOffset = edgeThickness * (1.0f - (i + 1.0f) / gradientSteps);

        // Gradient color: light gray (outer) to dark gray (inner)
        float grayValueOuter = 0.8f - 0.4f * t;  // Adjust to control lightness
        float grayValueInner = 0.8f - 0.4f * (t + 1.5f / gradientSteps);

        glBegin(GL_QUADS);
        glColor3f(grayValueOuter, grayValueOuter, grayValueOuter);
        glVertex2f(right - barWidth - outerOffset, bottom - outerOffset);
        glVertex2f(right + outerOffset, bottom - outerOffset);
        glVertex2f(right + outerOffset, bottom + barHeight + outerOffset);
        glVertex2f(right - barWidth - outerOffset, bottom + barHeight + outerOffset);

        glColor3f(grayValueInner, grayValueInner, grayValueInner);
        glVertex2f(right - barWidth - innerOffset, bottom - innerOffset);
        glVertex2f(right + innerOffset, bottom - innerOffset);
        glVertex2f(right + innerOffset, bottom + barHeight + innerOffset);
        glVertex2f(right - barWidth - innerOffset, bottom + barHeight + innerOffset);
        glEnd();
    }

    // Render the background of the bar
    glColor3f(0.0f, 0.0f, 0.0f);
    glBegin(GL_QUADS);
    glVertex2f(right - barWidth, bottom);
    glVertex2f(right, bottom);
    glVertex2f(right, bottom + barHeight);
    glVertex2f(right - barWidth, bottom + barHeight);
    glEnd();

    // Render the filled portion of the bar
    glColor3f(0.0f, 0.5f, 1.0f); // Blue color for depth
    glBegin(GL_QUADS);
    glVertex2f(right - barWidth, bottom);
    glVertex2f(right, bottom);
    glVertex2f(right, bottom + barHeight * normalizedDepth);
    glVertex2f(right - barWidth, bottom + barHeight * normalizedDepth);
    glEnd();

    // Render depth text below the progress bar
    std::ostringstream oss;
    oss << "Depth: " << static_cast<int>(submarineDepth) << "m";
    std::string depthText = oss.str();

    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

    // Adjust position and scale
    float textX = width * 0.81f;   // Move to the left
    float textY = height * 0.22f;  // Adjust to appear below the bar
    float textScale = 0.8f;        // Make the text smaller

    renderText(textShaderID, textX, textY, depthText, textScale);

    glDisable(GL_BLEND);
}



void drawNeedle(float aspectRatio) {
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

    // Needle parameters
    float angle = fmod(sonarTime * 40.0f, 360.0f); // Rotates over time
    float radius = 0.5f;                           // Needle length
    float sweepAngle = 30.0f;                      // Sweep angle in degrees

    // Convert angles to radians
    float startAngle = (angle - sweepAngle / 2.0f) * 3.14159f / 180.0f;
    float endAngle = (angle + sweepAngle / 2.0f) * 3.14159f / 180.0f;

    // Draw the fan-shaped needle with fading
    glPushMatrix();
    glScalef(1.0f / aspectRatio, 1.0f, 1.0f); // Correct aspect ratio for the needle
    glBegin(GL_TRIANGLE_FAN);

    // Center vertex - mostly transparent overall
    glColor4f(0.0f, 1.0f, 0.0f, 0.2f); // Very transparent green
    glVertex2f(0.0f, 0.0f);            // Center of the radar

    // Loop to create the fan shape
    for (float a = startAngle; a <= endAngle; a += 0.1f) {
        // Calculate alpha based on angle: fully transparent at startAngle, more opaque at endAngle
        float distance = (a - startAngle) / (endAngle - startAngle); // Normalize distance (0.0 to 1.0)
        float alpha = 0.f + (distance * 0.6f); // Starts at 20% transparency, goes to 70%

        // Set the color with calculated alpha
        glColor4f(0.0f, 1.0f, 0.0f, alpha);
        glVertex2f(cos(a) * radius, sin(a) * radius);
    }

    glEnd();
    glPopMatrix();
}


void drawSonar(float deltaTime, int width, int height) {
    // Increment animation time
    sonarTime += deltaTime;

    // Calculate aspect ratio
    float aspectRatio = static_cast<float>(width) / height;

    // Sonar parameters
    float sonarRadius = 0.5f; // Radius of the sonar
    float x = 0.0f, y = 0.0f; // Center of the sonar

    // Draw the ring around the sonar
    float ringThickness = sonarRadius * 0.1f; // Increase the thickness
    float outerRadius = sonarRadius + ringThickness;
    float innerRadius = sonarRadius;

    int gradientSteps = 20; // Number of gradient steps for smoothness

    glPushMatrix();
    glScalef(1.0f / aspectRatio, 1.0f, 1.0f); // Correct aspect ratio for a perfect circle

    for (int i = 0; i < gradientSteps; ++i) {
        // Calculate the current radius and color for this step
        float currentRadius = innerRadius + (outerRadius - innerRadius) * (i / (float)gradientSteps);
        float alpha = 1.0f; // Keep the alpha fixed for a solid ring
        float grayValue = 0.1f + 0.8f * (i / (float)gradientSteps); // Transition from dark gray to lighter gray

        glColor4f(grayValue, grayValue, grayValue, alpha);
        glBegin(GL_TRIANGLE_STRIP);
        for (int angle = 0; angle <= 360; angle += 10) {
            float theta = angle * 3.14159f / 180.0f;
            glVertex2f(x + cos(theta) * currentRadius, y + sin(theta) * currentRadius);
            glVertex2f(x + cos(theta) * (currentRadius - (outerRadius - innerRadius) / gradientSteps),
                y + sin(theta) * (currentRadius - (outerRadius - innerRadius) / gradientSteps));
        }
        glEnd();
    }
    glPopMatrix();

    // Draw the sonar's black background circle
    glPushMatrix();
    glScalef(1.0f / aspectRatio, 1.0f, 1.0f); // Correct aspect ratio for a perfect circle
    glColor3f(0.0f, 0.0f, 0.0f); // Black color
    glBegin(GL_TRIANGLE_FAN);
    glVertex2f(x, y); // Center
    for (int i = 0; i <= 360; i++) {
        float angle = i * 3.14159f / 180.0f;
        glVertex2f(cos(angle) * sonarRadius, sin(angle) * sonarRadius); // Radius
    }
    glEnd();
    glPopMatrix();
    // Draw a single expanding pulse ring
    if (radarEnabled)
    {
	    glEnable(GL_BLEND);
	    glPushMatrix();
	    glScalef(1.0f / aspectRatio, 1.0f, 1.0f); // Correct aspect ratio for the ring

	    float ringLifetime = 2.0f;               // Time it takes for the ring to fade and reset
	    float timeInCycle = fmod(sonarTime, ringLifetime); // Time since the start of the current ring
	    float ringRadius = timeInCycle / ringLifetime * sonarRadius; // Expanding radius
	    float ringAlpha = 1.0f - (timeInCycle / ringLifetime);       // Fading effect

	    if (ringRadius < sonarRadius) {
		    glColor4f(0.5f, 1.0f, 0.5f, ringAlpha * 0.5f); // Light green with transparency
		    glBegin(GL_LINE_LOOP);
		    for (int i = 0; i <= 360; i++) {
			    float angle = i * 3.14159f / 180.0f;
			    glVertex2f(cos(angle) * ringRadius, sin(angle) * ringRadius);
		    }
		    glEnd();
	    }
    }

    glPopMatrix();

    // If the radar is enabled, draw the needle and spawn dots
    if (radarEnabled) {
        // Draw the green needle
        drawNeedle(aspectRatio);

        // Add a red dot every 10 seconds
        if (sonarTime - lastDotSpawnTime >= 10.0f) {
            lastDotSpawnTime = sonarTime;

            // Minimum and maximum spawn distances
            float minDistance = 0.2f; // 20% of the needle's length
            float maxDistance = 0.5f; // Maximum radius

            // Spawn one dot at a random position along the needle
            float randomPos = minDistance + static_cast<float>(rand()) / RAND_MAX * (maxDistance - minDistance);
            float angle = fmod(sonarTime * 40.0f, 360.0f);             // Needle angle
            float dotX = cos(angle * 3.14159f / 180.0f) * randomPos;
            float dotY = sin(angle * 3.14159f / 180.0f) * randomPos;
            redDots.push_back({ dotX, dotY, sonarTime });
        }
    }
    glDisable(GL_BLEND);
    // Draw the red dots (already spawned ones will fade out as per lifetime)
    drawDots(aspectRatio);
}

void drawDots(float aspectRatio) {
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA); // Enable blending for fade effect

    for (size_t i = 0; i < redDots.size(); ++i) {
        float elapsedTime = sonarTime - redDots[i].creationTime;
        if (elapsedTime < 2.0f) { // Visible for 2 seconds
            float alpha = 1.0f - (elapsedTime / 2.0f); // Fade out effect

            glPushMatrix();
            glScalef(1.0f / aspectRatio, 1.0f, 1.0f); // Correct aspect ratio for dots

            // Draw a circle with transparency gradient
            float dotRadius = 0.02f; // Radius of the dot
            int numSegments = 36;    // Number of segments to approximate the circle

            glBegin(GL_TRIANGLE_FAN);
            // Center vertex: Full opacity
            glColor4f(1.0f, 0.0f, 0.0f, alpha);
            glVertex2f(redDots[i].x, redDots[i].y);

            // Outer vertices: Gradual fade to transparency
            for (int j = 0; j <= numSegments; ++j) {
                float angle = j * 2.0f * 3.14159f / numSegments;
                float dx = cos(angle) * dotRadius;
                float dy = sin(angle) * dotRadius;

                // Decrease alpha towards the outer edge
                float edgeAlpha = alpha * (1.0f - (sqrt(dx * dx + dy * dy) / dotRadius));
                glColor4f(1.0f, 0.0f, 0.0f, edgeAlpha); // Gradual transparency
                glVertex2f(redDots[i].x + dx, redDots[i].y + dy);
            }
            glEnd();

            glPopMatrix();
        }
        else {
            redDots.erase(redDots.begin() + i);
            --i;
        }
    }

    glDisable(GL_BLEND); // Disable blending after rendering
}





void drawButton(int width, int height) {
    // Button parameters
    float buttonRadius = 0.05f; // Radius of the button
    float buttonX = 0.5f;       // Position slightly to the right of the sonar
    float buttonY = -0.5f;      // Position slightly below the sonar

    // Get aspect ratio
    float aspectRatio = static_cast<float>(width) / height;

    // Draw the ring around the button
    float ringThickness = buttonRadius * 0.4f; // Same thickness as lamp ring
    float outerRadius = buttonRadius + ringThickness;
    float innerRadius = buttonRadius;

    int gradientSteps = 20; // Number of gradient steps for smoothness

    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

    // Apply transformation to maintain circular shape
    glPushMatrix();
    glScalef(1.0f / aspectRatio, 1.0f, 1.0f); // Correct aspect ratio for the button

    for (int i = 0; i < gradientSteps; ++i) {
        float currentRadius = innerRadius + (outerRadius - innerRadius) * (i / (float)gradientSteps);
        float alpha = 1.0f; // Fixed alpha for solid ring
        float grayValue = 0.1f + 0.8f * (i / (float)gradientSteps); // Gradient from dark gray to lighter gray

        glColor4f(grayValue, grayValue, grayValue, alpha);
        glBegin(GL_TRIANGLE_STRIP);
        for (int angle = 0; angle <= 360; angle += 10) {
            float theta = angle * 3.14159f / 180.0f;
            glVertex2f(buttonX + cos(theta) * currentRadius, buttonY + sin(theta) * currentRadius);
            glVertex2f(buttonX + cos(theta) * (currentRadius - (outerRadius - innerRadius) / gradientSteps),
                buttonY + sin(theta) * (currentRadius - (outerRadius - innerRadius) / gradientSteps));
        }
        glEnd();
    }

    // Draw the solid button circle (dark blue)
    glColor3f(0.0f, 0.0f, 0.5f); // Dark blue color
    glBegin(GL_TRIANGLE_FAN);
    glVertex2f(buttonX, buttonY); // Center of the button
    for (int angle = 0; angle <= 360; angle += 10) {
        float theta = angle * 3.14159f / 180.0f;
        glVertex2f(buttonX + cos(theta) * buttonRadius, buttonY + sin(theta) * buttonRadius);
    }
    glEnd();

    glPopMatrix();
    glDisable(GL_BLEND);
}


bool isMouseInButton(GLFWwindow* window, int width, int height) {
    double mouseX, mouseY;
    glfwGetCursorPos(window, &mouseX, &mouseY);

    // Convert mouse position to normalized device coordinates
    float normalizedX = static_cast<float>(mouseX) / width * 2.0f - 1.0f;
    float normalizedY = 1.0f - static_cast<float>(mouseY) / height * 2.0f;

    // Define button bounds based on the updated position and radius
    float buttonX = 0.5f;
    float buttonY = -0.5f;
    float buttonRadius = 0.05f;

    // Adjust normalizedX for aspect ratio
    float aspectRatio = static_cast<float>(width) / height;
    normalizedX *= aspectRatio;

    // Check if the mouse is within the button's circular bounds
    float distance = sqrt(pow(normalizedX - buttonX, 2) + pow(normalizedY - buttonY, 2));
    return distance <= buttonRadius;
}


unsigned int compileShader(const char* source, GLenum type) {
    unsigned int shader = glCreateShader(type);
    glShaderSource(shader, 1, &source, nullptr);
    glCompileShader(shader);

    int success;
    glGetShaderiv(shader, GL_COMPILE_STATUS, &success);
    if (!success) {
        char infoLog[512];
        glGetShaderInfoLog(shader, 512, nullptr, infoLog);
        std::cerr << "ERROR::SHADER::COMPILATION_FAILED\n" << infoLog << std::endl;
    }
    return shader;
}

unsigned int createTextShaderProgram() {
    const char* vertexShaderSource = R"(
        #version 330 core
        layout(location = 0) in vec4 vertex;
        out vec2 TexCoords;
        uniform mat4 projection;
        void main() {
            gl_Position = projection * vec4(vertex.xy, 0.0, 1.0);
            TexCoords = vertex.zw;
        }
    )";

    const char* fragmentShaderSource = R"(
        #version 330 core
        in vec2 TexCoords;
        out vec4 color;
        uniform sampler2D text;
        uniform vec3 textColor;
        void main() {
            float alpha = texture(text, TexCoords).r;
            color = vec4(textColor, alpha);
        }
    )";

    unsigned int vertexShader = compileShader(vertexShaderSource, GL_VERTEX_SHADER);
    unsigned int fragmentShader = compileShader(fragmentShaderSource, GL_FRAGMENT_SHADER);

    unsigned int shaderProgram = glCreateProgram();
    glAttachShader(shaderProgram, vertexShader);
    glAttachShader(shaderProgram, fragmentShader);
    glLinkProgram(shaderProgram);

    int success;
    glGetProgramiv(shaderProgram, GL_LINK_STATUS, &success);
    if (!success) {
        char infoLog[512];
        glGetProgramInfoLog(shaderProgram, 512, nullptr, infoLog);
        std::cerr << "ERROR::SHADER::PROGRAM::LINKING_FAILED\n" << infoLog << std::endl;
    }

    glDeleteShader(vertexShader);
    glDeleteShader(fragmentShader);

    return shaderProgram;
}
void drawIndicatorLamp(float x, float y, float radius, bool isRed, bool isGreen) {
    static double lastBlinkTime = 0.0; // Last time the blink state changed
    static bool visible = true;       // Visibility state of the lamp
    double currentTime = glfwGetTime();

    // Toggle visibility every 0.5 seconds
    if (currentTime - lastBlinkTime > 0.5) {
        visible = !visible;
        lastBlinkTime = currentTime;
    }

    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

    // Determine lamp color based on blinking state
    float r = 0.0f, g = 0.0f, b = 0.0f; // Default off (gray)
    if (isRed && visible) {
        r = 1.0f; g = 0.0f; b = 0.0f; // Red when visible
    }
    else if (isGreen) {
        r = 0.0f; g = 1.0f; b = 0.0f; // Green for safe state
    }

    // Get the framebuffer size to calculate aspect ratio
    int framebufferWidth, framebufferHeight;
    glfwGetFramebufferSize(glfwGetCurrentContext(), &framebufferWidth, &framebufferHeight);
    float aspectRatio = static_cast<float>(framebufferWidth) / framebufferHeight;

    // Save the current matrix state
    glPushMatrix();

    // Ensure the lamp remains a circle regardless of screen aspect ratio
    glScalef(1.0f / aspectRatio, 1.0f, 1.0f);

    //ring
    float ringThickness = radius * 0.4f; // Increase the thickness
    float outerRadius = radius + ringThickness;
    float innerRadius = radius;

    int gradientSteps = 20; // Number of gradient steps for smoothness

    for (int i = 0; i < gradientSteps; ++i) {
        // Calculate the current radius and color for this step
        float currentRadius = innerRadius + (outerRadius - innerRadius) * (i / (float)gradientSteps);
        float alpha = 1.0f; // Keep the alpha fixed for a solid ring
        float grayValue = 0.1f + 0.8f * (i / (float)gradientSteps); // Transition from dark gray to lighter gray

        glColor4f(grayValue, grayValue, grayValue, alpha);
        glBegin(GL_TRIANGLE_STRIP);
        for (int angle = 0; angle <= 360; angle += 10) {
            float theta = angle * 3.14159f / 180.0f;
            glVertex2f(x + cos(theta) * currentRadius, y + sin(theta) * currentRadius);
            glVertex2f(x + cos(theta) * (currentRadius - (outerRadius - innerRadius) / gradientSteps),
                y + sin(theta) * (currentRadius - (outerRadius - innerRadius) / gradientSteps));
        }
        glEnd();
    }

    // Draw the glow effect (only when the lamp is red and visible)
    if (isRed && visible) {
        int glowLayers = 15;          // Number of layers for the glow
        float glowRadius = radius * 10.0f; // Maximum glow radius
        for (int i = glowLayers; i > 0; --i) {
            float currentRadius = radius + (glowRadius - radius) * (i / (float)glowLayers);
            float alpha = 0.1f * (i / (float)glowLayers); // Decreasing opacity for each layer

            glColor4f(r, g, b, alpha);
            glBegin(GL_TRIANGLE_FAN);
            glVertex2f(x, y); // Center of the glow
            for (int angle = 0; angle <= 360; angle += 10) {
                float theta = angle * 3.14159f / 180.0f;
                glVertex2f(x + cos(theta) * currentRadius, y + sin(theta) * currentRadius);
            }
            glEnd();
        }
    }

    // Draw the solid lamp circle
    glColor3f(r, g, b); // Use the determined color
    glBegin(GL_TRIANGLE_FAN);
    glVertex2f(x, y); // Center of the lamp
    for (int angle = 0; angle <= 360; angle += 10) {
        float theta = angle * 3.14159f / 180.0f;
        glVertex2f(x + cos(theta) * radius, y + sin(theta) * radius);
    }
    glEnd();

    // Restore the matrix state
    glPopMatrix();

    glDisable(GL_BLEND);
}






void drawOxygenBar(int width, int height) {
    float normalizedOxygen = oxygenLevel / 100.0f; // Normalize oxygen level (0 to 1)

    float barWidth = 0.05f;
    float barHeight = 0.8f;
    float left = -0.8f; // Position the bar on the left side
    float bottom = -0.4f;

    // Draw gradient edges around the bar
    float edgeThickness = 0.015f; // Thickness of the gradient edges
    int gradientSteps = 10;      // Number of gradient steps for smoothness

    for (int i = 0; i < gradientSteps; ++i) {
        float t = i / (float)gradientSteps; // Normalize step (0.0 to 1.0)
        float outerOffset = edgeThickness * (1.0f - t); // Shrink outward to inward
        float innerOffset = edgeThickness * (1.0f - (i + 1.0f) / gradientSteps);

        // Gradient color: light gray (outer) to dark gray (inner)
        float grayValueOuter = 0.8f - 0.4f * t;  // Adjust to control lightness
        float grayValueInner = 0.8f - 0.4f * (t + 1.5f / gradientSteps);

        glBegin(GL_QUADS);
        glColor3f(grayValueOuter, grayValueOuter, grayValueOuter);
        glVertex2f(left - outerOffset, bottom - outerOffset);
        glVertex2f(left + barWidth + outerOffset, bottom - outerOffset);
        glVertex2f(left + barWidth + outerOffset, bottom + barHeight + outerOffset);
        glVertex2f(left - outerOffset, bottom + barHeight + outerOffset);

        glColor3f(grayValueInner, grayValueInner, grayValueInner);
        glVertex2f(left - innerOffset, bottom - innerOffset);
        glVertex2f(left + barWidth + innerOffset, bottom - innerOffset);
        glVertex2f(left + barWidth + innerOffset, bottom + barHeight + innerOffset);
        glVertex2f(left - innerOffset, bottom + barHeight + innerOffset);
        glEnd();
    }

    // Render the background of the bar
    glColor3f(0.0f, 0.0f, 0.0f);
    glBegin(GL_QUADS);
    glVertex2f(left, bottom);
    glVertex2f(left + barWidth, bottom);
    glVertex2f(left + barWidth, bottom + barHeight);
    glVertex2f(left, bottom + barHeight);
    glEnd();

    // Render the filled portion of the bar
    glColor3f(0.0f, 0.5f, 1.0f); // Blue color for oxygen
    glBegin(GL_QUADS);
    glVertex2f(left, bottom);
    glVertex2f(left + barWidth, bottom);
    glVertex2f(left + barWidth, bottom + barHeight * normalizedOxygen);
    glVertex2f(left, bottom + barHeight * normalizedOxygen);
    glEnd();

    // Lamp Position and Radius
    float lightX = left + barWidth / 2.0f - 0.60f; // Center X position of the lamp
    float lightY = bottom + barHeight + 0.11f; // Slightly above the bar
    float lampRadius = 0.05f; // Radius of the lamp

    // Draw the lamp using the new function
    drawIndicatorLamp(lightX, lightY, lampRadius, redLightOn, greenLightOn);

    // Render Text
    float textScale = 0.8f;
    GLint textColorLocation = glGetUniformLocation(textShaderID, "textColor");

    if (oxygenLevel <= 25.0f) {
        // Blinking logic
        static double lastBlinkTime = 0.0;
        static bool visible = true;
        double currentTime = glfwGetTime();
        if (currentTime - lastBlinkTime > 0.5) {
            visible = !visible;
            lastBlinkTime = currentTime;
        }

        if (visible) {
            glEnable(GL_BLEND);
            glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

            glUseProgram(textShaderID);
            glUniform3f(textColorLocation, 1.0f, 0.0f, 0.0f); // Set red color for warning

            float textX1 = width * 0.03f;
            float textY1 = height * 0.22f;
            renderText(textShaderID, textX1, textY1, "LOW OXYGEN", textScale);

            float textX2 = textX1;
            float textY2 = textY1 - 30.0f;
            renderText(textShaderID, textX2, textY2, "LEVEL", textScale);

            glDisable(GL_BLEND);
        }
    }
    else if (oxygenLevel > 75.0f) {
        glEnable(GL_BLEND);
        glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

        glUseProgram(textShaderID);
        glUniform3f(textColorLocation, 0.0f, 1.0f, 0.0f); // Set green color for safe

        float textX1 = width * 0.03f;
        float textY1 = height * 0.22f;
        renderText(textShaderID, textX1, textY1, "Oxygen Level", textScale);

        float textX2 = textX1;
        float textY2 = textY1 - 30.0f;
        renderText(textShaderID, textX2, textY2, "Sufficient", textScale);

        glDisable(GL_BLEND);
    }

    // Reset text color to white for other elements
    glUseProgram(textShaderID);
    glUniform3f(textColorLocation, 1.0f, 1.0f, 1.0f);
}







unsigned int loadTexture(const char* path) {
    unsigned int textureID;
    glGenTextures(1, &textureID);

    int width, height, nrChannels;
    stbi_set_flip_vertically_on_load(true);

    unsigned char* data = stbi_load(path, &width, &height, &nrChannels, 0);
    if (data) {
        GLenum format = (nrChannels == 4) ? GL_RGBA : GL_RGB;

        glBindTexture(GL_TEXTURE_2D, textureID);
        glTexImage2D(GL_TEXTURE_2D, 0, format, width, height, 0, format, GL_UNSIGNED_BYTE, data);
        glGenerateMipmap(GL_TEXTURE_2D);

        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    }
    else {
        std::cerr << "Failed to load texture: " << path << std::endl;
    }
    stbi_image_free(data);

    return textureID;
}
void initializeBackground() {
    float vertices[] = {
        // positions   // texture coords
        -1.0f,  1.0f,  0.0f, 1.0f,
        -1.0f, -1.0f,  0.0f, 0.0f,
         1.0f, -1.0f,  1.0f, 0.0f,

        -1.0f,  1.0f,  0.0f, 1.0f,
         1.0f, -1.0f,  1.0f, 0.0f,
         1.0f,  1.0f,  1.0f, 1.0f
    };

    glGenVertexArrays(1, &backgroundVAO);
    glGenBuffers(1, &backgroundVBO);

    glBindVertexArray(backgroundVAO);

    glBindBuffer(GL_ARRAY_BUFFER, backgroundVBO);
    glBufferData(GL_ARRAY_BUFFER, sizeof(vertices), vertices, GL_STATIC_DRAW);

    glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 4 * sizeof(float), (void*)0);
    glEnableVertexAttribArray(0);

    glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, 4 * sizeof(float), (void*)(2 * sizeof(float)));
    glEnableVertexAttribArray(1);

    glBindVertexArray(0);

    backgroundTexture = loadTexture("res/finalbackground.png");
    backgroundShader = createShaderProgram("background.vert", "background.frag");
}
void renderBackground() {
    glUseProgram(backgroundShader);
    glUniform1i(glGetUniformLocation(backgroundShader, "backgroundTexture"), 0);

    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, backgroundTexture);
    glUniform1i(glGetUniformLocation(backgroundShader, "backgroundTexture"), 0);

    glBindVertexArray(backgroundVAO);
    glDrawArrays(GL_TRIANGLES, 0, 6);
    glBindVertexArray(0);

    glUseProgram(0);
}
unsigned int createShaderProgram(const char* vertexPath, const char* fragmentPath) {
    std::string vertexCode, fragmentCode;
    std::ifstream vShaderFile, fShaderFile;

    vShaderFile.open(vertexPath);
    fShaderFile.open(fragmentPath);
    std::stringstream vShaderStream, fShaderStream;
    vShaderStream << vShaderFile.rdbuf();
    fShaderStream << fShaderFile.rdbuf();
    vShaderFile.close();
    fShaderFile.close();

    vertexCode = vShaderStream.str();
    fragmentCode = fShaderStream.str();

    const char* vShaderCode = vertexCode.c_str();
    const char* fShaderCode = fragmentCode.c_str();

    unsigned int vertexShader = compileShader(vShaderCode, GL_VERTEX_SHADER);
    unsigned int fragmentShader = compileShader(fShaderCode, GL_FRAGMENT_SHADER);

    unsigned int shaderProgram = glCreateProgram();
    glAttachShader(shaderProgram, vertexShader);
    glAttachShader(shaderProgram, fragmentShader);
    glLinkProgram(shaderProgram);

    int success;
    glGetProgramiv(shaderProgram, GL_LINK_STATUS, &success);
    if (!success) {
        char infoLog[512];
        glGetProgramInfoLog(shaderProgram, 512, nullptr, infoLog);
        std::cerr << "ERROR::SHADER::PROGRAM::LINKING_FAILED\n" << infoLog << std::endl;
    }

    glDeleteShader(vertexShader);
    glDeleteShader(fragmentShader);

    return shaderProgram;
}

int main() {
    // Initialize GLFW
    if (!glfwInit()) {
        std::cerr << "Failed to initialize GLFW!" << std::endl;
        return -1;
    }

    GLFWwindow* window = glfwCreateWindow(1280, 720, "Submarine Dashboard", nullptr, nullptr);
    if (!window) {
        std::cerr << "Failed to create GLFW window!" << std::endl;
        glfwTerminate();
        return -1;
    }
    glfwMakeContextCurrent(window);

    // Initialize GLEW
    if (glewInit() != GLEW_OK) {
        std::cerr << "Failed to initialize GLEW!" << std::endl;
        return -1;
    }
    std::cout << "GLEW initialized successfully." << std::endl;

    initializeBackground();

    // Initialize FreeType
    if (!initializeFreeType("res/Arial.ttf")) {
        std::cerr << "Failed to initialize FreeType!" << std::endl;
        return -1;
    }
    std::cout << "FreeType initialized successfully." << std::endl;

    initializeTextRendering();
    std::cout << "Text rendering initialized." << std::endl;
    textShaderID = createTextShaderProgram();
    glm::mat4 projection = glm::ortho(0.0f, static_cast<float>(1280), 0.0f, static_cast<float>(720));
    glUseProgram(textShaderID);
    glUniformMatrix4fv(glGetUniformLocation(textShaderID, "projection"), 1, GL_FALSE, glm::value_ptr(projection));

    const double frameDuration = 1.0 / 60.0; // 60 FPS
    double lastTime = glfwGetTime();

    // Game loop
    while (!glfwWindowShouldClose(window)) {
        double startTime = glfwGetTime();

        float deltaTime = static_cast<float>(startTime - lastTime);
        lastTime = startTime;

        glClear(GL_COLOR_BUFFER_BIT);

        // Render the background first
        renderBackground();

        // Oxygen depletion or refill logic
        if (submarineDepth > 0.0f) {
            oxygenLevel -= oxygenDepletionRate * deltaTime; // Decrease oxygen level
            if (oxygenLevel < 0.0f) oxygenLevel = 0.0f;    // Clamp to 0%
        }
        else {
            oxygenLevel += oxygenDepletionRate * deltaTime; // Increase oxygen level
            if (oxygenLevel > 100.0f) oxygenLevel = 100.0f; // Clamp to 100%
        }

        // Handle warning and light status
        if (oxygenLevel <= 25.0f) {
            redLightOn = true;
            greenLightOn = false;
        }
        else if (oxygenLevel > 75.0f) {
            redLightOn = false;
            greenLightOn = true;
        }
        else {
            redLightOn = false;
            greenLightOn = false;
        }

        // Handle input
        handleInput(window, 1280, 720, deltaTime);

        // Draw other elements
        drawSonar(deltaTime, 1280, 720);
        drawButton(1280, 720);
        drawDepthIndicator(1280, 720);
        drawOxygenBar(1280, 720);

        // Draw the text in the bottom-left corner
        float textScale = 0.7f; // Adjust scale as needed
        float textX = 10.0f;    // Offset from the left screen edge
        float textY = 20.0f;    // Offset from the bottom screen edge
        std::string nameText = "Veljko Puzovic RA 169/2021";

        glEnable(GL_BLEND);
        glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
        glUseProgram(textShaderID);

        // Set the text color to white
        glUniform3f(glGetUniformLocation(textShaderID, "textColor"), 1.0f, 1.0f, 1.0f);

        // Render the text
        renderText(textShaderID, textX, textY, nameText, textScale);

        glDisable(GL_BLEND);

        glfwSwapBuffers(window);
        glfwPollEvents();

        // Limit FPS
        double endTime = glfwGetTime();
        double frameTime = endTime - startTime;
        if (frameTime < frameDuration) {
            glfwWaitEventsTimeout(frameDuration - frameTime);
        }
    }

    glfwTerminate();
    return 0;
}
