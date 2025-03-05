#pragma once
#include"Scene.h";
#include "EBO.h"
#include "VAO.h"
#include "Camera.h"
#include "Texture.h"
#include "Shader.h"
#include "Model.h"
#include "ASSIMP.cpp"
#include "Text.h"

class RayScene : public Scene {
public:
    unsigned int SCREEN_WIDTH = 1024;
    unsigned int SCREEN_HEIGHT = 1024;

    Shader computeShader;
    Shader shader;
    Shader copyAccumShader;
    Shader textShader;

    Texture tex;
    Texture oldTex;
    Texture averageTex;

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