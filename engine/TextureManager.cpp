#define STB_IMAGE_IMPLEMENTATION
#include "TextureManager.h"

GLuint TextureManager::loadTexture(const std::string& path) {
    if (textures.count(path)) return textures[path];
    
    int w, h, nrChannels;
    unsigned char* data = stbi_load(path.c_str(), &w, &h, &nrChannels, 0);
    if (!data) {
        printf("[Engine] Failed to load texture: %s\n", path.c_str());
        return 0;
    }
    
    GLuint texture;
    glGenTextures(1, &texture);
    glBindTexture(GL_TEXTURE_2D, texture);
    
    // param
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);	
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    
    int format = (nrChannels == 4) ? GL_RGBA : GL_RGB;
    glTexImage2D(GL_TEXTURE_2D, 0, format, w, h, 0, format, GL_UNSIGNED_BYTE, data);
    
    stbi_image_free(data);
    
    textures[path] = texture;
    printf("[Engine] Loaded texture: %s (ID: %d)\n", path.c_str(), texture);
    return texture;
}

