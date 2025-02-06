#pragma once
#include "Text.h"
#include<glm/glm.hpp>
#include<glm/gtc/matrix_transform.hpp>
#include<glm/gtc/type_ptr.hpp>
#include<glm/gtx/rotate_vector.hpp>
#include<glm/gtx/vector_angle.hpp>

    GLUTText::GLUTText(int height, int width, const char* font) :
        SCREEN_WIDTH(width), 
        SCREEN_HEIGHT(height){
        initText(font);
    }


    int GLUTText::initText(const char* font) {

        VAO.Bind();
        VBO.SetData(sizeof(float) * 6 * 4, NULL, GL_DYNAMIC_DRAW);
        VAO.LinkAttrib(VBO, 0, 4, GL_FLOAT, 4 * sizeof(float), 0);
        
        VBO.Unbind();
        VAO.Unbind();

        FT_Error error;
        FT_Library ft;
        if (FT_Init_FreeType(&ft)) {
            std::cerr << "Could not initialize FreeType Library" << std::endl;
            return -1;
        }

        FT_Face face;
        if (FT_New_Face(ft, font, 0, &face)) {
            std::cerr << "Failed to load font" << std::endl;
            return -1;
        }

        FT_Set_Pixel_Sizes(face, 0, 12); // Set font size (height in pixels)
        glPixelStorei(GL_UNPACK_ALIGNMENT, 1); // disable byte-alignment restriction

        for (GLubyte c = 0; c < 128; c++) {
            if (FT_Load_Char(face, c, FT_LOAD_RENDER)) {
                std::cerr << "Failed to load Glyph" << std::endl;
                continue;
            }

            error = FT_Render_Glyph(face->glyph, FT_RENDER_MODE_NORMAL);

            // Generate texture for each character
            GLuint texture;
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

            // Texture options
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);

            // Store character information
            Character character = {
                texture,
                glm::ivec2(face->glyph->bitmap.width, face->glyph->bitmap.rows),
                glm::ivec2(face->glyph->bitmap_left, face->glyph->bitmap_top),
                static_cast<GLuint>(face->glyph->advance.x)
            };
            Characters.insert(std::pair<GLchar, Character>(c, character));
        }
        glBindTexture(GL_TEXTURE_2D, 0); // Unbind texture
        FT_Done_Face(face);
        FT_Done_FreeType(ft);
    }

    void GLUTText::RenderText(Shader& shader, std::string text, float x, float y, float scale, glm::vec3 color) {
        shader.Activate();
        glm::mat4 projection = glm::ortho(0.0f, static_cast<float>(SCREEN_WIDTH), 0.0f, static_cast<float>(SCREEN_HEIGHT));

        glUniformMatrix4fv(glGetUniformLocation(shader.ID, "projection"), 1, GL_FALSE, glm::value_ptr(projection));
        glUniform3f(glGetUniformLocation(shader.ID, "textColor"), color.x, color.y, color.z);
        glUniform1i(glGetUniformLocation(shader.ID, "text"), 10);

        glActiveTexture(GL_TEXTURE0+10);
        VAO.Bind();

        for (char c : text) {
            Character ch = Characters[c];

            float xpos = x + ch.bearing.x * scale;
            float ypos = y - (ch.size.y - ch.bearing.y);

            float w = ch.size.x * scale;
            float h = ch.size.y * scale;

            float vertices[6][4] = {
                { xpos,     ypos + h,   0.0f, 0.0f },
                { xpos,     ypos,       0.0f, 1.0f },
                { xpos + w, ypos,       1.0f, 1.0f },

                { xpos,     ypos + h,   0.0f, 0.0f },
                { xpos + w, ypos,       1.0f, 1.0f },
                { xpos + w, ypos + h,   1.0f, 0.0f }
            };

            glBindTexture(GL_TEXTURE_2D, ch.textureID);
            glBindBuffer(GL_ARRAY_BUFFER, VBO.ID);
            glBufferSubData(GL_ARRAY_BUFFER, 0, sizeof(vertices), vertices);

            glDrawArrays(GL_TRIANGLES, 0, 6);
            x += (ch.advance >> 6) * scale;
        }

        glBindVertexArray(0);
        glBindTexture(GL_TEXTURE_2D, 0);
    }

