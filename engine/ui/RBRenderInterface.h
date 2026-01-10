//
//  RBRenderInterface.h rbashkort 01/10/2026
//

#pragma once
#include <RmlUi/Core/RenderInterface.h>
#include "../thirdparty/stb/stb_image.h"
#include <GL/gl.h>
#include <vector>

struct CompiledGeometry {
    std::vector<Rml::Vertex> vertices;
    std::vector<int> indices;
};

class RBRenderInterface : public Rml::RenderInterface {
public:
    RBRenderInterface() {}

    Rml::CompiledGeometryHandle CompileGeometry(Rml::Span<const Rml::Vertex> vertices, Rml::Span<const int> indices) override 
    {
        CompiledGeometry* geometry = new CompiledGeometry();
        geometry->vertices.assign(vertices.begin(), vertices.end());
        geometry->indices.assign(indices.begin(), indices.end());
        return (Rml::CompiledGeometryHandle)geometry;
    }

    void RenderGeometry(Rml::CompiledGeometryHandle handle, Rml::Vector2f translation, Rml::TextureHandle texture) override 
    {
        CompiledGeometry* geometry = (CompiledGeometry*)handle;
        if (!geometry) return;

        glPushMatrix();
        glTranslatef(translation.x, translation.y, 0);

        glEnableClientState(GL_VERTEX_ARRAY);
        glEnableClientState(GL_COLOR_ARRAY);
        
        // Указываем OpenGL на наши массивы в памяти
        glVertexPointer(2, GL_FLOAT, sizeof(Rml::Vertex), &geometry->vertices[0].position);
        glColorPointer(4, GL_UNSIGNED_BYTE, sizeof(Rml::Vertex), &geometry->vertices[0].colour);

        if (texture) {
            glEnable(GL_TEXTURE_2D);
            glBindTexture(GL_TEXTURE_2D, (GLuint)texture);
            glEnableClientState(GL_TEXTURE_COORD_ARRAY);
            glTexCoordPointer(2, GL_FLOAT, sizeof(Rml::Vertex), &geometry->vertices[0].tex_coord);
        } else {
            glDisable(GL_TEXTURE_2D);
        }

        glDrawElements(GL_TRIANGLES, (GLsizei)geometry->indices.size(), GL_UNSIGNED_INT, &geometry->indices[0]);

        glDisableClientState(GL_VERTEX_ARRAY);
        glDisableClientState(GL_COLOR_ARRAY);
        glDisableClientState(GL_TEXTURE_COORD_ARRAY);
        glDisable(GL_TEXTURE_2D);
        
        glPopMatrix();
    }

    void ReleaseGeometry(Rml::CompiledGeometryHandle handle) override 
    {
        CompiledGeometry* geometry = (CompiledGeometry*)handle;
        delete geometry;
    }

    // --- Scissor ---
    
    void EnableScissorRegion(bool enable) override {
        if (enable) glEnable(GL_SCISSOR_TEST);
        else glDisable(GL_SCISSOR_TEST);
    }

        void SetScissorRegion(Rml::Rectanglei region) override {
        GLint viewport[4];
        glGetIntegerv(GL_VIEWPORT, viewport);
        
        glScissor(region.Left(), viewport[3] - (region.Top() + region.Height()), region.Width(), region.Height());
    }

    Rml::TextureHandle LoadTexture(Rml::Vector2i& texture_dimensions, const Rml::String& source) override 
    {
        int w, h, channels;
        unsigned char* data = stbi_load(source.c_str(), &w, &h, &channels, 4);
        
        if (!data) {
            printf("[RmlUi] Failed to load texture: %s\n", source.c_str());
            return 0; // 0 = ошибка
        }

        texture_dimensions.x = w;
        texture_dimensions.y = h;

        GLuint textureId;
        glGenTextures(1, &textureId);
        glBindTexture(GL_TEXTURE_2D, textureId);

        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
        
        glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, w, h, 0, GL_RGBA, GL_UNSIGNED_BYTE, data);
        
        stbi_image_free(data);
        glBindTexture(GL_TEXTURE_2D, 0);

        return (Rml::TextureHandle)textureId;
    }

    Rml::TextureHandle GenerateTexture(Rml::Span<const Rml::byte> source, Rml::Vector2i source_dimensions) override 
    {
        GLuint tex;
        glGenTextures(1, &tex);
        glBindTexture(GL_TEXTURE_2D, tex);
        glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, source_dimensions.x, source_dimensions.y, 
                     0, GL_RGBA, GL_UNSIGNED_BYTE, source.data());
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
        return (Rml::TextureHandle)tex;
    }

    void ReleaseTexture(Rml::TextureHandle texture) override {
        GLuint tex = (GLuint)texture;
        glDeleteTextures(1, &tex);
    }
};
