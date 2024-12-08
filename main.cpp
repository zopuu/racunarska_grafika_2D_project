#include <iostream>
#include <string>
#include <fstream>
#include <stdexcept>
#include <vector>
#include <cmath>
#include <random>
#include <GL/glew.h>
#include <GLFW/glfw3.h>

#define STB_IMAGE_IMPLEMENTATION
#include <chrono>

#include "stb_image.h"
#include <corecrt_math_defines.h>
#include <map>

#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>

#include <ft2build.h>
#include <thread>

#include FT_FREETYPE_H

GLuint textShader; // This provides a definition for textShader
GLuint shaderProgram;

// For glow effects




struct AngleRecord {
    float angle;
    float time;
};
std::vector<AngleRecord> angleHistory;
float trailDuration = 0.5f; // how many seconds the trail lasts


// Window dimensions
const unsigned int SCR_WIDTH = 1280;
const unsigned int SCR_HEIGHT = 720;

bool sonarOn = true;
float sonarRadius = 250.0f;
float sonarCenterX = 640.0f;
float sonarCenterY = 360.0f;
float sonarRotation = 0.0f;
float sonarPulseTime = 0.0f;

// Red dots data
struct RedDot {
    float x;
    float y;
    float spawnTime;
};
std::vector<RedDot> redDots;
float nextDotSpawn = 0.0f;

float deltaTime = 0.0f;
float lastFrame = 0.0f;

float currentDepth = 0.0f; // Current depth in meters, 0 to 250.
float currentOxygen = 1.0f; // 100% oxygen at start

// For text rendering
struct Character {
    GLuint TextureID;  // ID handle of the glyph texture
    glm::ivec2 Size;   // Size of glyph
    glm::ivec2 Bearing;// Offset from baseline to left/top of glyph
    GLuint Advance;    // Offset to advance to next glyph
};

std::map<GLchar, Character> Characters;
GLuint textVAO, textVBO;


void LoadFont(const char* fontPath, GLuint shaderProgram) {
    std::cout << "Loading font from: " << fontPath << std::endl;
    FT_Library ft;
    if (FT_Init_FreeType(&ft)) {
        std::cerr << "ERROR: Could not init FreeType Library\n";
        return;
    }

    FT_Face face;
    if (FT_New_Face(ft, fontPath, 0, &face)) {
        std::cerr << "ERROR: Failed to load font at path: " << fontPath << std::endl;
        FT_Done_FreeType(ft);
        return;
    }
    std::cout << "Font loaded successfully.\n";

    FT_Set_Pixel_Sizes(face, 0, 48);
    glPixelStorei(GL_UNPACK_ALIGNMENT, 1);

    // Clear Characters before loading
    Characters.clear();

    for (GLubyte c = 0; c < 128; c++) {
        if (FT_Load_Char(face, c, FT_LOAD_RENDER)) {
            std::cerr << "ERROR::FREETYTPE: Failed to load Glyph " << (int)c << std::endl;
            continue;
        }
        GLuint texture;
        glGenTextures(1, &texture);
        glBindTexture(GL_TEXTURE_2D, texture);
        glTexImage2D(
            GL_TEXTURE_2D, 0, GL_RED,
            face->glyph->bitmap.width,
            face->glyph->bitmap.rows,
            0, GL_RED, GL_UNSIGNED_BYTE,
            face->glyph->bitmap.buffer
        );
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);

        Character character = {
            texture,
            glm::ivec2(face->glyph->bitmap.width,face->glyph->bitmap.rows),
            glm::ivec2(face->glyph->bitmap_left,face->glyph->bitmap_top),
            (GLuint)face->glyph->advance.x
        };
        Characters.insert(std::pair<GLchar, Character>(c, character));
    }

    FT_Done_Face(face);
    FT_Done_FreeType(ft);

    glGenVertexArrays(1, &textVAO);
    glGenBuffers(1, &textVBO);
    glBindVertexArray(textVAO);
    glBindBuffer(GL_ARRAY_BUFFER, textVBO);
    glBufferData(GL_ARRAY_BUFFER, sizeof(GLfloat) * 6 * 4, NULL, GL_DYNAMIC_DRAW);
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(0, 4, GL_FLOAT, GL_FALSE, 4 * sizeof(GLfloat), (void*)0);
    glBindBuffer(GL_ARRAY_BUFFER, 0);
    glBindVertexArray(0);

    std::cout << "Characters loaded: " << Characters.size() << std::endl;
    if (Characters.size() == 0) {
        std::cerr << "ERROR: No characters loaded, text won't render.\n";
    }
}
void RenderText(GLuint shader, std::string text, GLfloat x, GLfloat y, GLfloat scale, glm::vec3 color) {
    std::cout << "RenderText called with text: \"" << text << "\" at (" << x << "," << y << ") scale: " << scale << std::endl;
    glUseProgram(shader);
    GLint colorLoc = glGetUniformLocation(shader, "uColor");
    glUniform3f(colorLoc, color.x, color.y, color.z);
    std::cout << "Set text color: (" << color.x << "," << color.y << "," << color.z << ")\n";

    glActiveTexture(GL_TEXTURE0);
    glBindVertexArray(textVAO);

    if (Characters.size() == 0) {
        std::cerr << "ERROR: Characters map is empty, cannot render text!\n";
    }

    for (auto c = text.begin(); c != text.end(); c++) {
        Character ch = Characters[*c];

        GLfloat xpos = x + ch.Bearing.x * scale;
        GLfloat ypos = y + (Characters['H'].Bearing.y - ch.Bearing.y) * scale;
        GLfloat w = ch.Size.x * scale;
        GLfloat h = ch.Size.y * scale;

        GLfloat vertices[6][4] = {
            { xpos,     ypos + h,   0.0, 0.0 },
            { xpos,     ypos,       0.0, 1.0 },
            { xpos + w, ypos,       1.0, 1.0 },

            { xpos,     ypos + h,   0.0, 0.0 },
            { xpos + w, ypos,       1.0, 1.0 },
            { xpos + w, ypos + h,   1.0, 0.0 }
        };
        glBindTexture(GL_TEXTURE_2D, ch.TextureID);
        glBindBuffer(GL_ARRAY_BUFFER, textVBO);
        glBufferSubData(GL_ARRAY_BUFFER, 0, sizeof(vertices), vertices);
        glBindBuffer(GL_ARRAY_BUFFER, 0);
        glDrawArrays(GL_TRIANGLES, 0, 6);

        x += (ch.Advance >> 6) * scale;
    }
    glBindVertexArray(0);
    glBindTexture(GL_TEXTURE_2D, 0);
}
void processInput(GLFWwindow* window) {
    if (glfwGetKey(window, GLFW_KEY_ESCAPE) == GLFW_PRESS)
        glfwSetWindowShouldClose(window, true);

    // Toggle sonar on/off if needed
    if (glfwGetKey(window, GLFW_KEY_O) == GLFW_PRESS) {
        sonarOn = !sonarOn;
    }

    // W increases depth, S decreases depth
    if (glfwGetKey(window, GLFW_KEY_W) == GLFW_PRESS) {
        currentDepth += 50.0f * deltaTime; // Adjust speed as desired
        if (currentDepth > 250.0f) currentDepth = 250.0f;
    }
    if (glfwGetKey(window, GLFW_KEY_S) == GLFW_PRESS) {
        currentDepth -= 50.0f * deltaTime;
        if (currentDepth < 0.0f) currentDepth = 0.0f;
    }
}


static std::string LoadFileToString(const std::string& filepath) {
    std::ifstream file(filepath, std::ios::in | std::ios::binary);
    if (!file)
        throw std::runtime_error("Failed to open file: " + filepath);
    std::string contents;
    file.seekg(0, std::ios::end);
    contents.resize((size_t)file.tellg());
    file.seekg(0, std::ios::beg);
    file.read(&contents[0], contents.size());
    file.close();
    return contents;
}

GLuint CompileShader(GLenum type, const std::string& source) {
    GLuint shader = glCreateShader(type);
    const char* srcCStr = source.c_str();
    glShaderSource(shader, 1, &srcCStr, NULL);
    glCompileShader(shader);
    int success;
    glGetShaderiv(shader, GL_COMPILE_STATUS, &success);
    if (!success) {
        char infoLog[512];
        glGetShaderInfoLog(shader, 512, NULL, infoLog);
        std::string err = "Shader compilation failed: ";
        err += infoLog;
        throw std::runtime_error(err);
    }
    return shader;
}

GLuint CreateShaderProgram(const std::string& vertPath, const std::string& fragPath) {
    std::string vertSrc = LoadFileToString(vertPath);
    std::string fragSrc = LoadFileToString(fragPath);
    GLuint vs = CompileShader(GL_VERTEX_SHADER, vertSrc);
    GLuint fs = CompileShader(GL_FRAGMENT_SHADER, fragSrc);

    GLuint program = glCreateProgram();
    glAttachShader(program, vs);
    glAttachShader(program, fs);
    glLinkProgram(program);

    int success;
    glGetProgramiv(program, GL_LINK_STATUS, &success);
    if (!success) {
        char infoLog[512];
        glGetProgramInfoLog(program, 512, NULL, infoLog);
        std::string err = "Program linking failed: ";
        err += infoLog;
        throw std::runtime_error(err);
    }

    glDeleteShader(vs);
    glDeleteShader(fs);
    return program;
}

GLuint LoadTexture(const std::string& path) {
    int width, height, nrChannels;
    unsigned char* data = stbi_load(path.c_str(), &width, &height, &nrChannels, 0);
    if (!data) {
        throw std::runtime_error("Failed to load texture: " + path);
    }

    GLuint texture;
    glGenTextures(1, &texture);
    glBindTexture(GL_TEXTURE_2D, texture);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    GLenum format = (nrChannels == 4) ? GL_RGBA : GL_RGB;
    glTexImage2D(GL_TEXTURE_2D, 0, format, width, height, 0, format, GL_UNSIGNED_BYTE, data);
    glGenerateMipmap(GL_TEXTURE_2D);
    stbi_image_free(data);
    return texture;
}


// Create a circle (triangle fan) VAO
GLuint createCircleVAO(int segments, float radius) {
    std::vector<float> vertices;
    vertices.push_back(0.0f); // center x
    vertices.push_back(0.0f); // center y
    vertices.push_back(0.0f); // center z

    for (int i = 0; i <= segments; i++) {
        float angle = (float)i / (float)segments * 2.0f * (float)M_PI;
        float x = cosf(angle) * radius;
        float y = sinf(angle) * radius;
        vertices.push_back(x);
        vertices.push_back(y);
        vertices.push_back(0.0f);
    }

    GLuint VAO, VBO;
    glGenVertexArrays(1, &VAO);
    glGenBuffers(1, &VBO);

    glBindVertexArray(VAO);
    glBindBuffer(GL_ARRAY_BUFFER, VBO);
    glBufferData(GL_ARRAY_BUFFER, vertices.size() * sizeof(float), vertices.data(), GL_STATIC_DRAW);

    // Position attribute
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 3 * sizeof(float), (void*)0);
    glEnableVertexAttribArray(0);

    glBindVertexArray(0);
    return VAO;
}

// Create a line VAO for the red kazaljka
// This line will be drawn from center to the outer edge of the circle
GLuint createLineVAO(float length) {
    float lineVertices[] = {
        0.0f, 0.0f, 0.0f,  // start at center
        length, 0.0f, 0.0f // end at radius
    };
    GLuint VAO, VBO;
    glGenVertexArrays(1, &VAO);
    glGenBuffers(1, &VBO);

    glBindVertexArray(VAO);
    glBindBuffer(GL_ARRAY_BUFFER, VBO);
    glBufferData(GL_ARRAY_BUFFER, sizeof(lineVertices), lineVertices, GL_STATIC_DRAW);

    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 3 * sizeof(float), (void*)0);
    glEnableVertexAttribArray(0);

    glBindVertexArray(0);
    return VAO;
}

GLuint sonarCircleVAO;
int sonarSegments = 64;
GLuint kazaljkaVAO;

int circleSegments = 64;
GLuint circleVAO;

float randFloat(float minVal, float maxVal) {
    static std::mt19937 rng((unsigned)std::random_device{}());
    std::uniform_real_distribution<float> dist(minVal, maxVal);
    return dist(rng);
}

extern GLuint textShader; // Assuming you have a global or external textShader for text rendering

void DrawDepthBar(GLint modelLoc, GLint colorLoc, GLint useTexLoc, float currentDepth) {
    // Position and size of the bar
    float barX = 1100.0f;   // Moved more to the right
    float barY = 200.0f;    // Adjust if needed
    float barWidth = 40.0f; // Wider bar
    float barHeight = 300.0f; // Taller bar

    // Compute fill ratio
    float depthRatio = currentDepth / 250.0f;
    if (depthRatio > 1.0f) depthRatio = 1.0f;
    if (depthRatio < 0.0f) depthRatio = 0.0f;

    // Filled portion height
    float fillHeight = barHeight * depthRatio;

    // Draw the bar background (gray)
    {
        glm::mat4 barModel = glm::mat4(1.0f);
        barModel = glm::translate(barModel, glm::vec3(barX, barY, 0.0f));
        barModel = glm::scale(barModel, glm::vec3(barWidth, barHeight, 1.0f));
        glUniformMatrix4fv(modelLoc, 1, GL_FALSE, glm::value_ptr(barModel));
        glUniform4f(colorLoc, 0.3f, 0.3f, 0.3f, 1.0f);
        glUniform1f(useTexLoc, 0.0f);

        float barQuad[] = {
            0.0f, 0.0f, 0.0f,
            1.0f, 0.0f, 0.0f,
            1.0f, 1.0f, 0.0f,
            0.0f, 1.0f, 0.0f
        };
        GLuint barVAO, barVBO;
        glGenVertexArrays(1, &barVAO);
        glGenBuffers(1, &barVBO);
        glBindVertexArray(barVAO);
        glBindBuffer(GL_ARRAY_BUFFER, barVBO);
        glBufferData(GL_ARRAY_BUFFER, sizeof(barQuad), barQuad, GL_STATIC_DRAW);
        glEnableVertexAttribArray(0);
        glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 3 * sizeof(float), (void*)0);
        glDrawArrays(GL_TRIANGLE_FAN, 0, 4);
        glDeleteBuffers(1, &barVBO);
        glDeleteVertexArrays(1, &barVAO);
    }

    // Draw the filled part (blue)
    {
        glm::mat4 fillModel = glm::mat4(1.0f);
        fillModel = glm::translate(fillModel, glm::vec3(barX, barY + (barHeight - fillHeight), 0.0f));
        fillModel = glm::scale(fillModel, glm::vec3(barWidth, fillHeight, 1.0f));
        glUniformMatrix4fv(modelLoc, 1, GL_FALSE, glm::value_ptr(fillModel));
        glUniform4f(colorLoc, 0.0f, 0.0f, 1.0f, 1.0f);
        glUniform1f(useTexLoc, 0.0f);

        float fillQuad[] = {
            0.0f,0.0f,0.0f,
            1.0f,0.0f,0.0f,
            1.0f,1.0f,0.0f,
            0.0f,1.0f,0.0f
        };
        GLuint fillVAO, fillVBO;
        glGenVertexArrays(1, &fillVAO);
        glGenBuffers(1, &fillVBO);
        glBindVertexArray(fillVAO);
        glBindBuffer(GL_ARRAY_BUFFER, fillVBO);
        glBufferData(GL_ARRAY_BUFFER, sizeof(fillQuad), fillQuad, GL_STATIC_DRAW);
        glEnableVertexAttribArray(0);
        glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 3 * sizeof(float), (void*)0);
        glDrawArrays(GL_TRIANGLE_FAN, 0, 4);
        glDeleteBuffers(1, &fillVBO);
        glDeleteVertexArrays(1, &fillVAO);
    }

    // Draw depth text
    glm::mat4 textProjection = glm::ortho(
        0.0f,
        (float)SCR_WIDTH,
        0.0f,  // bottom
        (float)SCR_HEIGHT, // top
        -1.0f, 1.0f
    );


    glUseProgram(textShader);
    std::cout << "Using textShader to draw depth text.\n";
    GLint texLoc = glGetUniformLocation(textShader, "uTexture");
    glUniform1i(texLoc, 0); // set texture sampler to texture unit 0
    std::cout << "Set uTexture to 0 on textShader.\n";

    int depthInt = (int)currentDepth;
    std::string depthText = "Depth: " + std::to_string(depthInt) + "m";
    glm::vec3 textColor(1.0f, 1.0f, 1.0f);

    float textX = barX -10.0f; // Align with bar or slightly offset
    float textY = barY + barHeight + 20.0f; // 20 pixels below the bottom of the bar
    float textScale = 0.7f;
    std::cout << "About to render text: " << depthText << " at (" << textX << "," << textY << ")\n";

    // Disable depth test if enabled:
    glDisable(GL_DEPTH_TEST);
    // Draw text on top
    RenderText(textShader, depthText, textX, textY, textScale, textColor);
    // Re-enable depth test if needed
    //glEnable(GL_DEPTH_TEST);
}
void DrawOxygenBar(GLint modelLoc, GLint colorLoc, GLint useTexLoc, float currentOxygen, float currentTime) {
    // Positions and dimensions as before
    float barX = 100.0f;
    float barY = 200.0f;
    float barWidth = 40.0f;
    float barHeight = 300.0f;

    // Clamp oxygen
    if (currentOxygen > 1.0f) currentOxygen = 1.0f;
    if (currentOxygen < 0.0f) currentOxygen = 0.0f;

    float fillHeight = barHeight * currentOxygen;

    // Draw background bar
    {
        glm::mat4 barModel = glm::mat4(1.0f);
        barModel = glm::translate(barModel, glm::vec3(barX, barY, 0.0f));
        barModel = glm::scale(barModel, glm::vec3(barWidth, barHeight, 1.0f));
        glUniformMatrix4fv(modelLoc, 1, GL_FALSE, glm::value_ptr(barModel));
        glUniform4f(colorLoc, 0.3f, 0.3f, 0.3f, 1.0f);
        glUniform1f(useTexLoc, 0.0f);

        float barQuad[] = {
            0.0f,0.0f,0.0f,
            1.0f,0.0f,0.0f,
            1.0f,1.0f,0.0f,
            0.0f,1.0f,0.0f
        };
        GLuint barVAO, barVBO;
        glGenVertexArrays(1, &barVAO);
        glGenBuffers(1, &barVBO);
        glBindVertexArray(barVAO);
        glBindBuffer(GL_ARRAY_BUFFER, barVBO);
        glBufferData(GL_ARRAY_BUFFER, sizeof(barQuad), barQuad, GL_STATIC_DRAW);
        glEnableVertexAttribArray(0);
        glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 3 * sizeof(float), (void*)0);
        glDrawArrays(GL_TRIANGLE_FAN, 0, 4);
        glDeleteBuffers(1, &barVBO);
        glDeleteVertexArrays(1, &barVAO);
    }

    // Draw filled portion
    {
        glm::mat4 fillModel = glm::mat4(1.0f);
        fillModel = glm::translate(fillModel, glm::vec3(barX, barY + (barHeight - fillHeight), 0.0f));
        fillModel = glm::scale(fillModel, glm::vec3(barWidth, fillHeight, 1.0f));
        glUniformMatrix4fv(modelLoc, 1, GL_FALSE, glm::value_ptr(fillModel));
        glUniform4f(colorLoc, 0.0f, 0.0f, 1.0f, 1.0f);
        glUniform1f(useTexLoc, 0.0f);

        float fillQuad[] = {
            0.0f,0.0f,0.0f,
            1.0f,0.0f,0.0f,
            1.0f,1.0f,0.0f,
            0.0f,1.0f,0.0f
        };
        GLuint fillVAO, fillVBO;
        glGenVertexArrays(1, &fillVAO);
        glGenBuffers(1, &fillVBO);
        glBindVertexArray(fillVAO);
        glBindBuffer(GL_ARRAY_BUFFER, fillVBO);
        glBufferData(GL_ARRAY_BUFFER, sizeof(fillQuad), fillQuad, GL_STATIC_DRAW);
        glEnableVertexAttribArray(0);
        glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 3 * sizeof(float), (void*)0);
        glDrawArrays(GL_TRIANGLE_FAN, 0, 4);
        glDeleteBuffers(1, &fillVBO);
        glDeleteVertexArrays(1, &fillVAO);
    }

    // Determine lamp and text state
    static bool wasRed = false;
    bool showRed = false;
    bool blinkRed = false;
    bool showGreen = false;

    if (currentOxygen < 0.25f) {
        wasRed = true;
        showRed = true;
        blinkRed = true;  // blink when below 25%
    }
    else if (currentOxygen > 0.75f) {
        wasRed = false;
        showGreen = true;
    }
    else {
        if (wasRed) {
            showRed = true;   // remain red stable if we were red before
            blinkRed = false; // stable red (no blink)
        }
        else {
            showGreen = true;
        }
    }

    // Positions and sizes for lamp and text
    float textX = barX - 10.0f;
    float textY = barY + barHeight + 50.0f;
    float lampX = barX + 10.0f;
    float lampY = barY - 50.0f;
    float lampRadius = 20.0f;
    float textScale = 0.7f;
    glm::vec3 textColor(1.0f, 1.0f, 1.0f);
    glm::vec4 lampColor(1.0f);
    std::string textToRender;

    // Blinking logic: 
    // We'll use a sine function to determine if lamp and text are "on" or "off"
    float blink = sin(currentTime * 5.0f);
    bool visible = true; // whether to draw on this blink frame

    if (showRed) {
        textColor = glm::vec3(1.0f, 0.0f, 0.0f);
        lampColor = glm::vec4(1.0f, 0.0f, 0.0f, 1.0f);
        textToRender = "Low Oxygen Level";

        if (blinkRed) {
            // Blink: turn off if blink < 0
            if (blink < 0.0f) {
                visible = false;
            }
        }
    }
    else if (showGreen) {
        textColor = glm::vec3(0.0f, 1.0f, 0.0f);
        lampColor = glm::vec4(0.0f, 1.0f, 0.0f, 1.0f);
        textToRender = "Enough Oxygen";
        // Green doesn't blink, always visible
    }
    else {
        // No mode selected, just return
        return;
    }

    // Draw text (if visible)
    if (visible) {
        glUseProgram(textShader);
        GLint texLoc = glGetUniformLocation(textShader, "uTexture");
        glUniform1i(texLoc, 0);
        glDisable(GL_DEPTH_TEST);
        // Render text only when visible
        RenderText(textShader, textToRender, textX, textY, textScale, textColor);
    }

    // Draw lamp (using basic shader)
    glUseProgram(shaderProgram);
    GLint modelLampLoc = glGetUniformLocation(shaderProgram, "uModel");
    GLint colorLampLoc = glGetUniformLocation(shaderProgram, "uColor");
    GLint useTexLampLoc = glGetUniformLocation(shaderProgram, "uUseTexture");

    // Draw the lamp as a small quad (or you can use a circle if desired)
    glm::mat4 lampModel = glm::mat4(1.0f);
    lampModel = glm::translate(lampModel, glm::vec3(lampX, lampY, 0.0f));
    lampModel = glm::scale(lampModel, glm::vec3(lampRadius, lampRadius, 1.0f));
    glUniformMatrix4fv(modelLampLoc, 1, GL_FALSE, glm::value_ptr(lampModel));
    glUniform4f(colorLampLoc, lampColor.r, lampColor.g, lampColor.b, lampColor.a * (visible ? 1.0f : 0.3f));
    glUniform1f(useTexLampLoc, 0.0f);

    float lampQuad[] = {
        0.0f,0.0f,0.0f,
        1.0f,0.0f,0.0f,
        1.0f,1.0f,0.0f,
        0.0f,1.0f,0.0f
    };
    GLuint lampVAO, lampVBO;
    glGenVertexArrays(1, &lampVAO);
    glGenBuffers(1, &lampVBO);
    glBindVertexArray(lampVAO);
    glBindBuffer(GL_ARRAY_BUFFER, lampVBO);
    glBufferData(GL_ARRAY_BUFFER, sizeof(lampQuad), lampQuad, GL_STATIC_DRAW);
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 3 * sizeof(float), (void*)0);
    glDrawArrays(GL_TRIANGLE_FAN, 0, 4);
    glDeleteBuffers(1, &lampVBO);
    glDeleteVertexArrays(1, &lampVAO);

    // Draw glow effect if red and visible
    if (showRed && visible) {
        int glowLayers = 15;
        float glowRadius = lampRadius * 10.0f;
        // We'll use a circle VAO similar to the sonar circle
        // Assume you have created something like:
        // circleVAO = createCircleVAO(circleSegments, 1.0f);
        // and have circleSegments defined globally.

        for (int i = glowLayers; i > 0; --i) {
            float layerRatio = (float)i / (float)glowLayers;
            float currentRadius = lampRadius + (glowRadius - lampRadius) * layerRatio;
            float layerAlpha = 0.1f * layerRatio;

            glm::mat4 glowModel = glm::mat4(1.0f);
            glowModel = glm::translate(glowModel, glm::vec3(lampX, lampY, 0.0f));
            glowModel = glm::scale(glowModel, glm::vec3(currentRadius, currentRadius, 1.0f));
            glUniformMatrix4fv(modelLampLoc, 1, GL_FALSE, glm::value_ptr(glowModel));
            glUniform4f(colorLampLoc, lampColor.r, lampColor.g, lampColor.b, layerAlpha);
            glUniform1f(useTexLampLoc, 0.0f);

            glBindVertexArray(circleVAO); // A VAO created for a circle (like sonar)
            glDrawArrays(GL_TRIANGLE_FAN, 0, circleSegments + 2);
        }
    }
}

void DrawSignature() {
    // Coordinates near bottom-left corner
    float x = 20.0f;
    float y = SCR_HEIGHT - 30.0f;
    float scale = 0.7f;
    glm::vec3 color(1.0f, 1.0f, 1.0f); // White text

    // Use textShader
    glUseProgram(textShader);
    // Render the signature text
    RenderText(textShader, "Veljko Puzovic RA 169/2021", x, y, scale, color);
}



int main() {
    // Initialize GLFW
    if (!glfwInit())
        return -1;

    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);

    GLFWwindow* window = glfwCreateWindow(SCR_WIDTH, SCR_HEIGHT, "Submarine Dashboard", NULL, NULL);
    if (!window)
    {
        glfwTerminate();
        return -1;
    }
    glfwMakeContextCurrent(window);

    // Initialize GLEW
    if (glewInit() != GLEW_OK) {
        std::cerr << "Failed to init GLEW" << std::endl;
        return -1;
    }

    glViewport(0, 0, SCR_WIDTH, SCR_HEIGHT);

    textShader = CreateShaderProgram("text.vert", "text.frag");
    std::cout << "Text shader created.\n";
    LoadFont("res/Arial.ttf", textShader);
    std::cout << "Font loaded.\n";

    // Set text projection
    glm::mat4 textProjection = glm::ortho(0.0f, (float)SCR_WIDTH, (float)SCR_HEIGHT, 0.0f, -1.0f, 1.0f);
    glUseProgram(textShader);
    GLint textProjLoc = glGetUniformLocation(textShader, "uProjection");
    glUniformMatrix4fv(textProjLoc, 1, GL_FALSE, glm::value_ptr(textProjection));
    std::cout << "Set uProjection for text.\n";
    GLint texLoc = glGetUniformLocation(textShader, "uTexture");
    glUniform1i(texLoc, 0);
    std::cout << "Set uTexture for text.\n";

    shaderProgram = CreateShaderProgram("basic.vert", "basic.frag");
    std::cout << "Basic shader created.\n";

    float currentOxygen = 1.0f;
    float oxygenChangeRate = 0.05f; // how fast oxygen changes per second

    // Full-screen quad for background
    float quadVertices[] = {
        // Positions                     // Texture Coords
        0.0f,        0.0f,        0.0f,  0.0f, 1.0f,
        0.0f,        (float)SCR_HEIGHT, 0.0f,  0.0f, 0.0f,
        (float)SCR_WIDTH, (float)SCR_HEIGHT, 0.0f,  1.0f, 0.0f,

        0.0f,        0.0f,        0.0f,  0.0f, 1.0f,
        (float)SCR_WIDTH, (float)SCR_HEIGHT, 0.0f,  1.0f, 0.0f,
        (float)SCR_WIDTH, 0.0f,    0.0f,  1.0f, 1.0f
    };


    GLuint VAO, VBO;
    glGenVertexArrays(1, &VAO);
    glGenBuffers(1, &VBO);

    glBindVertexArray(VAO);
    glBindBuffer(GL_ARRAY_BUFFER, VBO);
    glBufferData(GL_ARRAY_BUFFER, sizeof(quadVertices), quadVertices, GL_STATIC_DRAW);
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 5 * sizeof(float), (void*)0);
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, 5 * sizeof(float), (void*)(3 * sizeof(float)));
    glEnableVertexAttribArray(1);

    glBindVertexArray(0);

    GLuint backgroundTex = LoadTexture("res/background.png");

    float identity[16] = {
        1,0,0,0,
        0,1,0,0,
        0,0,1,0,
        0,0,0,1
    };

    // Create sonar geometry
    sonarCircleVAO = createCircleVAO(sonarSegments, sonarRadius);
    kazaljkaVAO = createLineVAO(sonarRadius);

    circleVAO = createCircleVAO(circleSegments, 1.0f); // unit circle, we will scale as needed

    // Enable blending for potential semi-transparent effects
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

    float lastFrame = 0.0f;

    // Target FPS and frame timing
    const double TARGET_FPS = 60.0;
    const double FRAME_TIME = 1.0 / TARGET_FPS;
    auto lastFrameTime = std::chrono::high_resolution_clock::now();

    while (!glfwWindowShouldClose(window)) {
        // Frame start timing
        auto startFrameTime = std::chrono::high_resolution_clock::now();
        double dt = std::chrono::duration<double>(startFrameTime - lastFrameTime).count();
        lastFrameTime = startFrameTime;

        float currentFrame = (float)glfwGetTime();
        deltaTime = currentFrame - lastFrame;
        lastFrame = currentFrame;

        processInput(window);

        // Update sonar rotation
        if (sonarOn) {
            sonarRotation += 50.0f * deltaTime; // rotate 50 deg/s
            if (sonarRotation > 360.0f) sonarRotation -= 360.0f;
        }
        if (sonarOn) {
            // Add current angle and time to history
            angleHistory.push_back({ sonarRotation, currentFrame });

            // Remove old angles
            // If angle is older than trailDuration seconds, remove it
            while (!angleHistory.empty() && (currentFrame - angleHistory.front().time) > trailDuration) {
                angleHistory.erase(angleHistory.begin());
            }
        }

        // Pulsating green
        sonarPulseTime += deltaTime;
        float pulse = (sin(sonarPulseTime * 2.0f) * 0.5f) + 0.5f;
        // pulse goes from 0 to 1. Use it to modulate green color between two shades
        float greenIntensity = 0.3f + pulse * 0.7f; // from 0.3 to 1.0 green

        // Spawn red dots at a fixed 7-second interval
        if (sonarOn && currentFrame > nextDotSpawn) {
            nextDotSpawn = currentFrame + 7.0f;  // spawn once every 7 seconds
            // Random position inside circle
            float r = sonarRadius * sqrtf(randFloat(0.0f, 1.0f));
            float angle = randFloat(0.0f, 2.0f * (float)M_PI);
            RedDot dot;
            dot.x = r * cosf(angle);
            dot.y = r * sinf(angle);
            dot.spawnTime = currentFrame; // record spawn time
            redDots.push_back(dot);
        }

        // Clear screen
        glClearColor(0.0f, 0.0f, 0.0f, 1.0f);
        glClear(GL_COLOR_BUFFER_BIT);




        // After creating the projection matrix
        glm::mat4 projection = glm::ortho(0.0f, (float)SCR_WIDTH, (float)SCR_HEIGHT, 0.0f, -1.0f, 1.0f);

        glUseProgram(shaderProgram);
        GLint projLoc = glGetUniformLocation(shaderProgram, "uProjection");
        GLint modelLoc = glGetUniformLocation(shaderProgram, "uModel");
        GLint colorLoc = glGetUniformLocation(shaderProgram, "uColor");
        GLint useTexLoc = glGetUniformLocation(shaderProgram, "uUseTexture");

        // Set the projection matrix once
        glUniformMatrix4fv(projLoc, 1, GL_FALSE, glm::value_ptr(projection));

        // Draw background
        glm::mat4 model = glm::mat4(1.0f);
        glUniformMatrix4fv(modelLoc, 1, GL_FALSE, glm::value_ptr(model));
        glUniform4f(colorLoc, 1.0f, 1.0f, 1.0f, 1.0f);
        glUniform1f(useTexLoc, 1.0f);

        glActiveTexture(GL_TEXTURE0);
        glBindTexture(GL_TEXTURE_2D, backgroundTex);

        glBindVertexArray(VAO);
        glDrawArrays(GL_TRIANGLES, 0, 6);

        // Now when you draw the sonar, it will use the same projection
        glm::mat4 sonarModel = glm::mat4(1.0f);
        sonarModel = glm::translate(sonarModel, glm::vec3(sonarCenterX, sonarCenterY, 0.0f));
        // ... set up color, etc., and draw


        // Draw sonar if on
        if (sonarOn) {
            // Draw green circle
            // Compute model matrix to position sonar at (sonarCenterX, sonarCenterY)
            float model[16] = {
                1,0,0,0,
                0,1,0,0,
                0,0,1,0,
                sonarCenterX,sonarCenterY,0,1
            };
            glUniformMatrix4fv(modelLoc, 1, GL_FALSE, model);
            glUniform1f(useTexLoc, 0.0f);
            glUniform4f(colorLoc, 0.0f, greenIntensity, 0.0f, 1.0f);

            glBindVertexArray(sonarCircleVAO);
            // draw triangle fan: 1 center + segments+1 edges = segments+2 vertices total
            glDrawArrays(GL_TRIANGLE_FAN, 0, sonarSegments + 2);

            // Draw red dots inside sonar
            for (size_t i = 0; i < redDots.size(); ) {
                RedDot& dot = redDots[i];
                float age = currentFrame - dot.spawnTime; // how long since spawned
                if (age > 2.0f) {
                    // dot has fully faded, remove it
                    redDots.erase(redDots.begin() + i);
                    continue; // don't increment i, vector shifted
                }
                else {
                    // Calculate alpha: 1.0 at spawn, 0.0 at 2 seconds
                    float alpha = 1.0f - (age / 2.0f);

                    float dotSize = 6.0f;
                    float dotModel[16] = {
                        dotSize,0,0,0,
                        0,dotSize,0,0,
                        0,0,1,0,
                        sonarCenterX + dot.x - (dotSize / 2.0f), sonarCenterY + dot.y - (dotSize / 2.0f),0,1
                    };
                    glUniformMatrix4fv(modelLoc, 1, GL_FALSE, dotModel);
                    glUniform4f(colorLoc, 1.0f, 0.0f, 0.0f, alpha);
                    // Use a simple quad for dot
                    float dotQuad[] = {
                        0.0f,0.0f,0.0f,
                        1.0f,0.0f,0.0f,
                        1.0f,1.0f,0.0f,
                        0.0f,1.0f,0.0f
                    };
                    GLuint dotVAO, dotVBO;
                    glGenVertexArrays(1, &dotVAO);
                    glGenBuffers(1, &dotVBO);
                    glBindVertexArray(dotVAO);
                    glBindBuffer(GL_ARRAY_BUFFER, dotVBO);
                    glBufferData(GL_ARRAY_BUFFER, sizeof(dotQuad), dotQuad, GL_STATIC_DRAW);
                    glEnableVertexAttribArray(0);
                    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 3 * sizeof(float), (void*)0);
                    glDrawArrays(GL_TRIANGLE_FAN, 0, 4);
                    glDeleteBuffers(1, &dotVBO);
                    glDeleteVertexArrays(1, &dotVAO);

                    i++; // move to next dot
                }
            }

            // Rotate line by sonarRotation around center
            if (sonarOn) {
                glUniform1f(useTexLoc, 0.0f);
            	float trailModel[16] = {
			        1,0,0,0,
			        0,1,0,0,
			        0,0,1,0,
			        sonarCenterX, sonarCenterY,0,1
                };
                glUniformMatrix4fv(modelLoc, 1, GL_FALSE, trailModel);

                // Iterate over angles
                for (size_t i = 0; i + 1 < angleHistory.size(); i++) {
                    AngleRecord& a1 = angleHistory[i + 1];
                    AngleRecord& a2 = angleHistory[i];

                    float age1 = currentFrame - a1.time;
                    float age2 = currentFrame - a2.time;

                    float alpha1 = 1.0f - (age1 / trailDuration);
                    float alpha2 = 1.0f - (age2 / trailDuration);
                    float alpha = (alpha1 + alpha2) * 0.5f;
                    alpha *= 0.5f; // Additional fade so even newest segments are not fully opaque

                    // Negate angle if needed based on previous logic
                    float angleRad1 = -a1.angle * (float)M_PI / 180.0f;
                    float angleRad2 = -a2.angle * (float)M_PI / 180.0f;

                    float x1 = sonarRadius * cosf(angleRad1);
                    float y1 = sonarRadius * sinf(angleRad1);
                    float x2 = sonarRadius * cosf(angleRad2);
                    float y2 = sonarRadius * sinf(angleRad2);

                    float triVertices[] = {
                        0.0f, 0.0f, 0.0f,
                        x2,   y2,   0.0f,
                        x1,   y1,   0.0f
                    };

                    GLuint triVAO, triVBO;
                    glGenVertexArrays(1, &triVAO);
                    glGenBuffers(1, &triVBO);
                    glBindVertexArray(triVAO);
                    glBindBuffer(GL_ARRAY_BUFFER, triVBO);
                    glBufferData(GL_ARRAY_BUFFER, sizeof(triVertices), triVertices, GL_DYNAMIC_DRAW);
                    glEnableVertexAttribArray(0);
                    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 3 * sizeof(float), (void*)0);

                    glUniform4f(colorLoc, 1.0f, 0.0f, 0.0f, alpha);

                    glDrawArrays(GL_TRIANGLES, 0, 3);

                    glDeleteBuffers(1, &triVBO);
                    glDeleteVertexArrays(1, &triVAO);
                }

                // Draw the main kazaljka line as before.
                float angleRad = sonarRotation * (float)M_PI / 180.0f;
                float c = cosf(angleRad);
                float s = sinf(angleRad);

                float rotModel[16] = {
                    c, -s, 0, 0,
                    s,  c, 0, 0,
                    0,  0, 1, 0,
                    sonarCenterX, sonarCenterY, 0, 1
                };

                glUniformMatrix4fv(modelLoc, 1, GL_FALSE, rotModel);
                glUniform4f(colorLoc, 1.0f, 0.0f, 0.0f, 1.0f);
                glBindVertexArray(kazaljkaVAO);
                glDrawArrays(GL_LINES, 0, 2);
            }

            // After drawing the trail, now draw the main kazaljka line as before
            float angleRad = sonarRotation * (float)M_PI / 180.0f;
            float c = cosf(angleRad);
            float s = sinf(angleRad);

            float rotModel[16] = {
                c, -s, 0, 0,
                s,  c, 0, 0,
                0,  0, 1, 0,
                sonarCenterX, sonarCenterY, 0, 1
            };

            glUniformMatrix4fv(modelLoc, 1, GL_FALSE, rotModel);
            glUniform4f(colorLoc, 1.0f, 0.0f, 0.0f, 1.0f);
            glBindVertexArray(kazaljkaVAO);
            glDrawArrays(GL_LINES, 0, 2);
        
        }
        DrawDepthBar(modelLoc, colorLoc, useTexLoc, currentDepth);
        // Oxygen logic:
        if (currentDepth > 0.0f) {
            // Submarine is underwater, oxygen decreases
            currentOxygen -= oxygenChangeRate * deltaTime;
        }
        else {
            // At surface, oxygen regenerates
            currentOxygen += oxygenChangeRate * deltaTime;
        }

        if (currentOxygen > 1.0f) currentOxygen = 1.0f;
        if (currentOxygen < 0.0f) currentOxygen = 0.0f;
        glUseProgram(shaderProgram);
        DrawOxygenBar(modelLoc, colorLoc, useTexLoc, currentOxygen, currentFrame);
        DrawSignature();


        glfwSwapBuffers(window);
        glfwPollEvents();

        // Frame end timing
        auto endFrameTime = std::chrono::high_resolution_clock::now();
        double frameDuration = std::chrono::duration<double>(endFrameTime - startFrameTime).count();
        double sleepTime = FRAME_TIME - frameDuration;
        if (sleepTime > 0.0) {
            std::this_thread::sleep_for(std::chrono::duration<double>(sleepTime));
        }
    }

    glDeleteProgram(shaderProgram);
    glfwTerminate();
    return 0;
}
