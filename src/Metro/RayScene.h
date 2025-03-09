#pragma once
#include"../Scene.h";
#include "../Core/EBO.h"
#include "../Core/VAO.h"
#include "../Core/Camera.h"
#include "../Core/Texture.h"
#include "../Core/Shader.h"
#include "../Core/Model.h"
#include "../Lib/ASSIMP.cpp"
#include "../Core/Text.h"

class RayScene : public Scene {
public:
    unsigned int SCREEN_WIDTH = 1400;
    unsigned int SCREEN_HEIGHT = 800;

    Shader computeShader;
    Shader shader;
    Shader copyAccumShader;
    Shader textShader;

    Texture tex;
    Texture biasTex;
    Texture oldTex;

    Texture metroplisDirectionsTex;
    Texture metroplisColorsTex;

    Camera camera;
    ASSModel model;

    VAO SceneVAO;
    std::unique_ptr<VBO> SceneVBO;
    std::unique_ptr<EBO> SceneEBO;

    GLUTText text;

    int Frame = 0;

    void AddSurfaces();
    void AddMeshes();

    void OnBufferSwap(Window& win) override;
    void OnWindowLoad(Window& win) override;
    void OnWindowClose(Window& win) override;

    RayScene(Window& win);
};