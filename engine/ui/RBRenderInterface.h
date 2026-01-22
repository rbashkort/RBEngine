#pragma once


#include "../glad.h" 
#include <RmlUi/Core/RenderInterface.h>
#include <RmlUi/Core/Vertex.h>
#include <GLFW/glfw3.h>
#include "../thirdparty/stb/stb_image.h" 
#include <vector>
#include <string>
#include <iostream>

#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>

// Структура для хранения скомпилированной геометрии
struct RmlCompiledGeometry {
    unsigned int VAO;
    unsigned int VBO;
    unsigned int EBO;
    int numIndices;
    unsigned int texture; // 0 если нет
};

class RBRenderInterface : public Rml::RenderInterface {
private:
    unsigned int shaderProgram = 0;
    int uModelLoc = -1;
    int uProjLoc = -1;
    int uTexLoc = -1;
    int uUseTexLoc = -1;

    int screenWidth = 800;
    int screenHeight = 600;

    void CreateShaders() {
        const char* vShaderSrc = R"(
            #version 330 core
            layout (location = 0) in vec2 aPos;
            layout (location = 1) in vec4 aColor;
            layout (location = 2) in vec2 aTexCoord;

            out vec4 FragColor;
            out vec2 TexCoord;

            uniform mat4 projection;
            uniform mat4 model;

            void main() {
                gl_Position = projection * model * vec4(aPos, 0.0, 1.0);
                FragColor = aColor; // RmlUi дает цвет уже в 0..255, но glVertexAttribPointer нормализует его
                TexCoord = aTexCoord;
            }
        )";

        const char* fShaderSrc = R"(
            #version 330 core
            out vec4 FinalColor;

            in vec4 FragColor;
            in vec2 TexCoord;

            uniform sampler2D uTexture;
            uniform bool uUseTex;

            void main() {
                if (uUseTex) {
                    FinalColor = texture(uTexture, TexCoord) * FragColor;
                } else {
                    FinalColor = FragColor;
                }
            }
        )";

        // Компиляция (упрощенная)
        unsigned int vs = glCreateShader(GL_VERTEX_SHADER);
        glShaderSource(vs, 1, &vShaderSrc, NULL);
        glCompileShader(vs);

        unsigned int fs = glCreateShader(GL_FRAGMENT_SHADER);
        glShaderSource(fs, 1, &fShaderSrc, NULL);
        glCompileShader(fs);

        shaderProgram = glCreateProgram();
        glAttachShader(shaderProgram, vs);
        glAttachShader(shaderProgram, fs);
        glLinkProgram(shaderProgram);

        glDeleteShader(vs);
        glDeleteShader(fs);

        uModelLoc = glGetUniformLocation(shaderProgram, "model");
        uProjLoc = glGetUniformLocation(shaderProgram, "projection");
        uTexLoc = glGetUniformLocation(shaderProgram, "uTexture");
        uUseTexLoc = glGetUniformLocation(shaderProgram, "uUseTex");
        
        printf("[UI] Shaders initialized.\n");
    }

public:
    RBRenderInterface() {
        // Шейдеры лучше инициализировать после создания контекста GL,
        // поэтому вызовем это позже или здесь, если контекст уже есть.
        // Но обычно интерфейс создается до окна.
        // Поэтому добавим флаг инициализации в RenderGeometry.
    }

    // Метод для обновления вьюпорта (вызывать из UIManager::render)
    void SetViewport(int w, int h) {
        screenWidth = w;
        screenHeight = h;
    }

    // --- RmlUi Methods ---

    Rml::CompiledGeometryHandle CompileGeometry(Rml::Span<const Rml::Vertex> vertices, Rml::Span<const int> indices) override 
    {
        if(shaderProgram == 0) CreateShaders(); // Lazy Init

        RmlCompiledGeometry* geometry = new RmlCompiledGeometry();
        geometry->numIndices = (int)indices.size();

        glGenVertexArrays(1, &geometry->VAO);
        glGenBuffers(1, &geometry->VBO);
        glGenBuffers(1, &geometry->EBO);

        glBindVertexArray(geometry->VAO);

        glBindBuffer(GL_ARRAY_BUFFER, geometry->VBO);
        glBufferData(GL_ARRAY_BUFFER, vertices.size() * sizeof(Rml::Vertex), vertices.data(), GL_STATIC_DRAW);

        glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, geometry->EBO);
        glBufferData(GL_ELEMENT_ARRAY_BUFFER, indices.size() * sizeof(int), indices.data(), GL_STATIC_DRAW);

        // Position (vec2 float)
        glEnableVertexAttribArray(0);
        glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, sizeof(Rml::Vertex), (const void*)offsetof(Rml::Vertex, position));

        // Color (vec4 ubyte -> normalized to float [0..1])
        glEnableVertexAttribArray(1);
        glVertexAttribPointer(1, 4, GL_UNSIGNED_BYTE, GL_TRUE, sizeof(Rml::Vertex), (const void*)offsetof(Rml::Vertex, colour));

        // TexCoord (vec2 float)
        glEnableVertexAttribArray(2);
        glVertexAttribPointer(2, 2, GL_FLOAT, GL_FALSE, sizeof(Rml::Vertex), (const void*)offsetof(Rml::Vertex, tex_coord));

        glBindVertexArray(0);
        glBindBuffer(GL_ARRAY_BUFFER, 0);

        return (Rml::CompiledGeometryHandle)geometry;
    }

    void RenderGeometry(Rml::CompiledGeometryHandle handle, Rml::Vector2f translation, Rml::TextureHandle texture) override 
    {
        RmlCompiledGeometry* geometry = (RmlCompiledGeometry*)handle;
        if (!geometry) return;

        if(shaderProgram == 0) CreateShaders(); // На всякий случай

        glUseProgram(shaderProgram);

        // Включаем смешивание для UI (прозрачность)
        glEnable(GL_BLEND);
        glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
        glDisable(GL_DEPTH_TEST);

        // Projection: Ortho (0,0 top-left)
        glm::mat4 projection = glm::ortho(0.0f, (float)screenWidth, (float)screenHeight, 0.0f, -100.0f, 100.0f);
        glUniformMatrix4fv(uProjLoc, 1, GL_FALSE, glm::value_ptr(projection));

        // Model: Translation
        glm::mat4 model = glm::translate(glm::mat4(1.0f), glm::vec3(translation.x, translation.y, 0.0f));
        glUniformMatrix4fv(uModelLoc, 1, GL_FALSE, glm::value_ptr(model));

        // Texture
        if (texture) {
            glActiveTexture(GL_TEXTURE0);
            glBindTexture(GL_TEXTURE_2D, (GLuint)texture);
            glUniform1i(uTexLoc, 0);
            glUniform1i(uUseTexLoc, true);
        } else {
            glUniform1i(uUseTexLoc, false);
        }

        // Draw
        glBindVertexArray(geometry->VAO);
        glDrawElements(GL_TRIANGLES, geometry->numIndices, GL_UNSIGNED_INT, 0);
        glBindVertexArray(0);
        
        // Сброс (на всякий)
        glUseProgram(0);
    }

    void ReleaseGeometry(Rml::CompiledGeometryHandle handle) override 
    {
        RmlCompiledGeometry* geometry = (RmlCompiledGeometry*)handle;
        glDeleteVertexArrays(1, &geometry->VAO);
        glDeleteBuffers(1, &geometry->VBO);
        glDeleteBuffers(1, &geometry->EBO);
        delete geometry;
    }

    // --- Scissor ---
    void EnableScissorRegion(bool enable) override {
        if (enable) glEnable(GL_SCISSOR_TEST);
        else glDisable(GL_SCISSOR_TEST);
    }

    void SetScissorRegion(Rml::Rectanglei region) override {
        // OpenGL Scissor (0,0 is Bottom-Left), RmlUi (0,0 is Top-Left)
        glScissor(region.Left(), screenHeight - (region.Top() + region.Height()), region.Width(), region.Height());
    }

    // --- Textures (Без изменений, но используем glad) ---
    Rml::TextureHandle LoadTexture(Rml::Vector2i& texture_dimensions, const Rml::String& source) override 
    {
        // ... (Твой старый код LoadTexture с stbi_load, он совместим с Modern GL) ...
        // Только убедись, что используешь функции GLAD (они такие же: glGenTextures, glTexImage2D)
        
        // Копирую для полноты, чтобы не потерялось:
        int w, h, channels;
        unsigned char* data = stbi_load(source.c_str(), &w, &h, &channels, 4);
        if (!data) return 0;

        texture_dimensions.x = w;
        texture_dimensions.y = h;

        GLuint textureId;
        glGenTextures(1, &textureId);
        glBindTexture(GL_TEXTURE_2D, textureId);
        glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, w, h, 0, GL_RGBA, GL_UNSIGNED_BYTE, data);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
        stbi_image_free(data);
        return (Rml::TextureHandle)textureId;
    }

    Rml::TextureHandle GenerateTexture(Rml::Span<const Rml::byte> source, Rml::Vector2i source_dimensions) override 
    {
        GLuint textureId;
        glGenTextures(1, &textureId);
        glBindTexture(GL_TEXTURE_2D, textureId);
        glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, source_dimensions.x, source_dimensions.y, 0, GL_RGBA, GL_UNSIGNED_BYTE, source.data());
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
        return (Rml::TextureHandle)textureId;
    }

    void ReleaseTexture(Rml::TextureHandle texture) override {
        GLuint id = (GLuint)texture;
        glDeleteTextures(1, &id);
    }
};
