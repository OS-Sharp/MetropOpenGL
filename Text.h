#pragma once
#include <ft2build.h>
#include <iostream>
#include <glad/glad.h>
#include FT_FREETYPE_H
#include<glm/glm.hpp>
#include<glm/gtc/matrix_transform.hpp>
#include<glm/gtc/type_ptr.hpp>
#include<glm/gtx/rotate_vector.hpp>
#include<glm/gtx/vector_angle.hpp>
#include <map>
#include "Shader.h"
#include "VAO.h"

class GLUTText {
    public:

        struct Character {
            GLuint textureID;  // ID handle of the glyph texture
            glm::ivec2 size;   // Size of glyph
            glm::ivec2 bearing; // Offset from baseline to left/top of glyph
            GLuint advance;    // Offset to advance to next glyph
        };

        std::map<GLchar, Character> Characters;
        VAO VAO;
        VBO VBO;
        int SCREEN_WIDTH, SCREEN_HEIGHT;

        int initText(const char* font);

        void RenderText(Shader& shader, std::string text, float x, float y, float scale, glm::vec3 color);

        GLUTText(int width, int height, const char* font);
}; 
