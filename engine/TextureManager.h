#pragma once
#include <map>
#include <string>
#include <GL/gl.h>
#include "../thirdparty/stb/stb_image.h"

class TextureManager {
    std::map<std::string, GLuint> textures;
public:
    GLuint loadTexture(const std::string& path);
    GLuint getTexture(const std::string& path);
};

