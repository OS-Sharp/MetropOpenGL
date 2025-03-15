#include "RayScene.h"
#include "ComputeStructures.h"
#include <chrono>
#include "../Core/Text.h"

// Global variables for frame timing and debug
int frameCount = 0;
float fps = 0.0f;
std::chrono::time_point<std::chrono::steady_clock> lastTime = std::chrono::steady_clock::now();
BVH sceneBVH;
int DEBUGMODE = 0;
const int DEBUGTEST = 1;
const int DEBUGTHRESHOLD = 30;
const int RAYSPERPIXEL = 1;
const int METROPLIS_MUTATIONS = 1;
const int BOUNCES = 6;
bool wasPressed = false;

// Render mode enumeration
enum RenderMode {
    PATH_TRACING = 0,
    METROPLIS = 1,
    PATH_TRACING_BIDIRECTIONAL = 2
};
const RenderMode renderMode = PATH_TRACING;

// Define workgroup and dispatch sizes
const int LAYOUT_SIZE_X = 8;
const int LAYOUT_SIZE_Y = 8;
const int METROPLIS_DISPATCH_X = 16;
const int METROPLIS_DISPATCH_Y = 16;

// Start time for FPS calculation
static auto startTime = std::chrono::steady_clock::now();

//
// RayScene class constructor and methods
//
RayScene::RayScene(Window& win) :
    SCREEN_WIDTH(win.width),
    SCREEN_HEIGHT(win.height),
    shader("shaders/default.vert", "shaders/default.frag"),
    computeShader("shaders/compute.comp"),
    copyAccumShader("shaders/accumulation.comp"),
    tex(SCREEN_WIDTH, SCREEN_HEIGHT, 0, 0),
    biasTex(SCREEN_WIDTH, SCREEN_HEIGHT, 1, 1),
    oldTex(SCREEN_WIDTH, SCREEN_HEIGHT, 2, 2),
    metroplisColorsTex(SCREEN_WIDTH, SCREEN_HEIGHT, 5, 5),
    metroplisDirectionsTex(SCREEN_WIDTH, SCREEN_HEIGHT, 6, 6),
    camera(SCREEN_WIDTH, SCREEN_HEIGHT, glm::vec3(0.0f, 0.0f, -5.0f)),
    textShader("shaders/text_vertex.vert", "shaders/text_fragment.frag"),
    text(SCREEN_WIDTH, SCREEN_HEIGHT, "fonts/Raleway-Black.ttf")
{
    // Setup shader storage buffer for per-thread BVH stack
    const int MAX_STACK_SIZE = 32;
    const int dispatchX = SCREEN_WIDTH / (renderMode == METROPLIS ? METROPLIS_DISPATCH_X : LAYOUT_SIZE_X);
    const int dispatchY = SCREEN_HEIGHT / (renderMode == METROPLIS ? METROPLIS_DISPATCH_Y : LAYOUT_SIZE_Y);
    const int localSizeX = 8;  // Must match shader's local_size_x
    const int localSizeY = 8;  // Must match shader's local_size_y

    int totalThreads = dispatchX * dispatchY * localSizeX * localSizeY;
    int bufferSize = MAX_STACK_SIZE * totalThreads * sizeof(int);

    GLuint bvhStackBuffer;
    glGenBuffers(1, &bvhStackBuffer);
    glBindBuffer(GL_SHADER_STORAGE_BUFFER, bvhStackBuffer);
    glBufferData(GL_SHADER_STORAGE_BUFFER, bufferSize, nullptr, GL_DYNAMIC_COPY);
    glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 20, bvhStackBuffer);
    glBindBuffer(GL_SHADER_STORAGE_BUFFER, 0);

    // Create a material and load a model into the scene BVH
    /*
    Material material;
    material.emmisionColor = glm::vec3(0.0f, 0.0f, 0.0f);
    material.emmisionStrength = glm::vec3(0, 0, 0);
    material.diffuseColor = glm::vec3(0.8f, 0.3f, 0.3f);
    material.specularChance = glm::vec3(0.2f, 0, 0);
    material.smoothness = glm::vec3(0.4f, 0, 0);

    ASSModel dragon("models/FilchCorridor.obj");
    std::vector<Triangle> dragonTris = dragon.ToTriangles(material, 0.001f, glm::vec3(0, 0, 0), shader, sceneBVH);
    */

    /*
    Material material;
    material.emmisionColor = glm::vec3(0.0f, 0.0f, 0.0f);
    material.emmisionStrength = glm::vec3(0, 0, 0);
    material.diffuseColor = glm::vec3(1.0f, 1.0f, 1.0f);
    material.specularChance = glm::vec3(0.8f, 0, 0);
    material.smoothness = glm::vec3(1.0f, 0, 0);

    ASSModel dragon("models/Dragon.obj");
    std::vector<Triangle> dragonTris = dragon.ToTriangles(material, 1.0f, glm::vec3(0, 0, 0), shader, sceneBVH);
    */

    
    Material material;
    material.emmisionColor = glm::vec3(0.0f, 0.0f, 0.0f);
    material.emmisionStrength = glm::vec3(0, 0, 0);
    material.diffuseColor = glm::vec3(1.0f, 1.0f, 1.0f);
    material.specularChance = glm::vec3(0.0f, 0, 0);
    material.smoothness = glm::vec3(0.0f, 0, 0);
    
    ASSModel dragon("models/hall01.obj");
    std::vector<Triangle> dragonTris = dragon.ToTriangles(material, 3.0f, glm::vec3(0, 0, 0), computeShader, sceneBVH);
    

    /*
    Material material;
    material.emmisionColor = glm::vec3(0.0f, 0.0f, 0.0f);
    material.emmisionStrength = glm::vec3(0, 0, 0);
    material.diffuseColor = glm::vec3(1.0f, 1.0f, 1.0f);
    material.specularChance = glm::vec3(0.0f, 0, 0);
    material.smoothness = glm::vec3(0.0f, 0, 0);
    
    ASSModel dragon("models/Sala_simplesobj.obj");
    std::vector<Triangle> dragonTris = dragon.ToTriangles(material, 4.0f, glm::vec3(40.0f, 0.0f, -60.0f), shader, sceneBVH);
    sceneBVH.AddModel(dragonTris, material);
    */
    AddMeshes();
    AddSurfaces();
}

void updateFPS() {
    frameCount++;
    auto currentTime = std::chrono::steady_clock::now();
    std::chrono::duration<float> elapsed = currentTime - lastTime;
    if (elapsed.count() >= 1.0f) { // Update every 1 second
        fps = frameCount / elapsed.count();
        frameCount = 0;
        lastTime = currentTime;
    }
}

//
// Updated AddMeshes() – note that we removed the old MeshInfo uploads
// and reassign the bindings for triangle, BVH, and model data
//
void RayScene::AddMeshes() {
    // Upload triangle data.
   // (Assuming sceneBVH.Triangles is of type BVHTriangle or compatible with Triangle)
    computeShader.StoreSSBO<Triangle>(sceneBVH.Triangles, 9, false);
    // (If you no longer need to upload a separate triangle count, omit it.)

    // Upload BVH node array to binding 11 and its count to binding 12.
    computeShader.StoreSSBO<BVHNode>(sceneBVH.FlatNodes, 11, false);
    computeShader.StoreSSBO<GLuint>(static_cast<GLuint>(sceneBVH.FlatNodes.size()), 12, false);
    // Upload model array to binding 13 and its count to binding 14.
    computeShader.StoreSSBO<BVHModel>(sceneBVH.Models, 13, false);
    computeShader.StoreSSBO<GLuint>(static_cast<GLuint>(sceneBVH.Models.size()), 14, false);
}

//
// Updated AddSurfaces() – the circles (spheres) and boxes are now uploaded
// to the new bindings (5–6 for circles; 7–8 for boxes)
//
void RayScene::AddSurfaces() {
    std::vector<TraceCircle> circles;

    TraceCircle circle2;
    Material material2;
    material2.emmisionColor = glm::vec3(5.0f, 5.0f, 5.0f);
    //material2.emmisionStrength = glm::vec3(4.0f, 4.0f, 4.0f);
    material2.emmisionStrength = glm::vec3(1.0f, 1.0f,1.0f);
    material2.diffuseColor = glm::vec3(1.0f, 1.0f, 1.0f);
    material2.specularChance = glm::vec3(0.0f, 0, 0);
    material2.smoothness = glm::vec3(0.0f, 0, 0);

    circle2.material = material2;
    //circle2.position = glm::vec3(35.0f , 40.0f, 0.0f);
    circle2.position = glm::vec3(0.0f, 10.0f, -60.0f);
    circle2.radius = 12.0f;

    circles.push_back(circle2);

    TraceCircle circle3;

    Material materialbrown;
    materialbrown.emmisionColor = glm::vec3(0.69f, 0.682f, 0.271f);
    //material2.emmisionStrength = glm::vec3(4.0f, 4.0f, 4.0f);
    materialbrown.emmisionStrength = glm::vec3(1.0f, 1.0f, 1.0f);
    materialbrown.diffuseColor = glm::vec3(0.69f, 0.682f, 0.271f);
    materialbrown.specularChance = glm::vec3(1.0f, 0, 0);
    materialbrown.smoothness = glm::vec3(1.0f, 0, 0);

    circle3.material = materialbrown;
    //circle2.position = glm::vec3(35.0f , 40.0f, 0.0f);
    circle3.position = glm::vec3(-40.0f, 7.0f, -55.0f);
    circle3.radius = 1.4f;

    circles.push_back(circle3);

    TraceCircle circleFar;
    circleFar.material = material2;
    circleFar.position = glm::vec3(0.0f, 60.0f, 0.0f);
    //circleFar.position = glm::vec3(5.0f, 00.0f, 20.0f);
    circleFar.radius = 10.0f;

    circles.push_back(circleFar);

    TraceCircle reflective;
    Material material3;
    material3.emmisionColor = glm::vec3(1.0f, 1.0f, 1.0f);
    material3.emmisionStrength = glm::vec3(0.0f);
    material3.diffuseColor = glm::vec3(0.0f);
    material3.specularChance = glm::vec3(0.4f, 0, 0);
    material3.smoothness = glm::vec3(1.0f, 0, 0);

    material3.opacity = glm::vec3(1.0f, 0, 0);

    reflective.material = material3;
    reflective.position = glm::vec3(0, 10.0f, 0.0f);
    reflective.radius = 7.0f;
    circles.push_back(reflective);

    TraceCircle reflective2;
    Material material4;
    material4.emmisionColor = glm::vec3(1.0f, 1.0f, 1.0f);
    material4.emmisionStrength = glm::vec3(0.0f);
    material4.diffuseColor = glm::vec3(0.0f);
    material4.specularChance = glm::vec3(0.9f, 0, 0);
    material4.smoothness = glm::vec3(1.0f, 0, 0);

    material4.opacity = glm::vec3(1.0f, 0, 0);

    reflective2.material = material4;
    reflective2.position = glm::vec3(0, 20.0f, 0.0f);
    reflective2.radius = 5.0f;

    circles.push_back(reflective2);

    std::vector<TraceDebugBox> boxes;
    TraceDebugBox box;
    Material matBox;
    matBox.emmisionColor = glm::vec3(1.0f, 1.0f, 1.0f);
    matBox.emmisionStrength = glm::vec3(0.0f, 0.0f, 0.0f);
    matBox.diffuseColor = glm::vec3(0.9f, 0.4f, 0.4f);
    matBox.specularChance = glm::vec3(0.0f, 0, 0);
    matBox.smoothness = glm::vec3(0.0f, 0, 0);
    matBox.opacity = glm::vec3(0.0f, 0, 0);
    box.material = matBox;
    box.position = glm::vec3(2.0f, 2.0f, 10.0f);
    box.size = glm::vec3(2.0f, 2.0f, 2.0f);
    //boxes.push_back(box);

    // Upload circles to binding 5 and their count to binding 6.
    computeShader.StoreSSBO<TraceCircle>(circles, 7, false);
    computeShader.StoreSSBO<GLuint>(static_cast<GLuint>(circles.size()), 8, false);
}


//
// OnBufferSwap() – Called on each buffer swap to update frame data, dispatch the compute shader,
// copy accumulated image data, update camera settings, and render the final output.
//
void RayScene::OnBufferSwap(Window& win) {
    auto currentTimePoint = std::chrono::steady_clock::now();
    std::chrono::duration<float> elapsedSeconds = currentTimePoint - startTime;
    float timeInSeconds = elapsedSeconds.count();

    // Toggle debug mode on key press (B key)
    static bool wasPressed = false;
    if (glfwGetKey(win.instance, GLFW_KEY_B) == GLFW_PRESS && !wasPressed) {
        DEBUGMODE = 1 - DEBUGMODE;
        wasPressed = true;
    }
    else if (glfwGetKey(win.instance, GLFW_KEY_B) == GLFW_RELEASE) {
        wasPressed = false;
    }

    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
    updateFPS();

    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

    SceneVAO.Bind();
    bool hasMoved = camera.Inputs(win.instance);
    camera.UpdateMatrix(45.0f, 0.1f, 100.0f);
    camera.Matrix(computeShader, "viewProj");

    if (hasMoved) {
        Frame = 0;
        GLfloat clearColor[4] = { 0.0f, 0.0f, 0.0f, 0.0f };
        GLfloat whiteClear[4] = { 1.0f, 1.0f, 1.0f, 1.0f };

        // Clear textures at mipmap level 0.
        glClearTexImage(tex.ID, 0, GL_RGBA, GL_FLOAT, clearColor);
        glClearTexImage(oldTex.ID, 0, GL_RGBA, GL_FLOAT, clearColor);
        glClearTexImage(biasTex.ID, 0, GL_RGBA, GL_FLOAT, clearColor);
        glClearTexImage(metroplisColorsTex.ID, 0, GL_RGBA, GL_FLOAT, clearColor);
        glClearTexImage(metroplisDirectionsTex.ID, 0, GL_RGBA, GL_FLOAT, clearColor);
    }

    glMemoryBarrier(GL_ALL_BARRIER_BITS);

    // Copy the current accumulation texture to the old texture
    glCopyImageSubData(tex.ID, GL_TEXTURE_2D, 0, 0, 0, 0,
        oldTex.ID, GL_TEXTURE_2D, 0, 0, 0, 0,
        SCREEN_WIDTH, SCREEN_HEIGHT, 1);

    // Update Frame and camera settings in SSBOs
    GLuint frame = computeShader.StoreSSBO<GLuint>(Frame, 4);
    Frame++;

    CameraSettings cameraSettings;
    cameraSettings.position = glm::vec3(camera.Position);
    cameraSettings.direction = glm::vec3(camera.Orientation);
    cameraSettings.fov = 90.0f;
    GLuint camSettings = computeShader.StoreSSBO<CameraSettings>(cameraSettings, 3);

    // Set shader uniform parameters
    computeShader.Activate();
    computeShader.SetParameterFloat(timeInSeconds, "uTime");
    computeShader.SetParameterColor(glm::vec3(1.0f), "SkyColourHorizon");
    computeShader.SetParameterColor(glm::vec3(0.08f, 0.37f, 0.73f), "SkyColourZenith");
    computeShader.SetParameterColor(glm::normalize(glm::vec3(1.0f, -0.5f, -1.0f)), "SunLightDirection");
    computeShader.SetParameterColor(glm::vec3(0.35f), "GroundColor");
    computeShader.SetParameterFloat(500.0f, "SunFocus");
    computeShader.SetParameterFloat(10.0f, "SunIntensity");
    computeShader.SetParameterFloat(0.0f, "SunThreshold");
    computeShader.SetParameterInt(DEBUGMODE, "DebugMode");
    computeShader.SetParameterFloat(0.2f, "SkyStrength");
    computeShader.SetParameterInt(BOUNCES, "NumberOfBounces");
    computeShader.SetParameterInt(RAYSPERPIXEL, "NumberOfRays");
    computeShader.SetParameterInt(METROPLIS_MUTATIONS, "NumberOfMutations");
    computeShader.SetParameterInt(DEBUGTHRESHOLD, "DebugThreshold");
    computeShader.SetParameterInt(DEBUGTEST, "DebugTest");
    computeShader.SetParameterInt(1, "BurnInSamples");

    int rMode = static_cast<int>(renderMode);
    computeShader.SetParameterInt(rMode, "RENDER_MODE");
    computeShader.SetParameterInt(SCREEN_WIDTH / METROPLIS_DISPATCH_X, "METROPLIS_DISPATCH_X");
    computeShader.SetParameterInt(SCREEN_HEIGHT / METROPLIS_DISPATCH_Y, "METROPLIS_DISPATCH_Y");

    glMemoryBarrier(GL_ALL_BARRIER_BITS);

    // Dispatch compute shader
    int gX = (renderMode == METROPLIS) ? (SCREEN_WIDTH / METROPLIS_DISPATCH_X) : (SCREEN_WIDTH / LAYOUT_SIZE_X);
    int gY = (renderMode == METROPLIS) ? (SCREEN_HEIGHT / METROPLIS_DISPATCH_Y) : (SCREEN_HEIGHT / LAYOUT_SIZE_Y);
    computeShader.Dispatch(gX, gY, 1);
    glMemoryBarrier(GL_ALL_BARRIER_BITS);

    // Bind textures for final rendering
    tex.texUnit(shader, "tex0", 0);
    biasTex.texUnit(shader, "tex1", 1);
    oldTex.texUnit(shader, "tex2", 2);

    shader.SetParameterInt(Frame, "Frame");
    shader.Activate();
    glDrawElements(GL_TRIANGLES, 6, GL_UNSIGNED_INT, 0);

    computeShader.DeleteSSBOs();

    textShader.Activate();
    // Render FPS text on the screen
    text.RenderText(textShader, "fps: " + std::to_string(static_cast<int>(fps)),
        25.0f, 25.0f, 1.0f, glm::vec3(1.0f));
}


//
// OnWindowLoad() – Called when the window is created to setup vertex objects and buffers.
//
void RayScene::OnWindowLoad(Window& win) {
    // Bind vertex array for scene quad
    SceneVAO.Bind();

    std::vector<Vertex> vertices = {
        Vertex(glm::vec3(-1.0f, -1.0f, 0.0f), glm::vec3(0.0f), glm::vec3(0.0f), glm::vec2(0.0f)),
        Vertex(glm::vec3(1.0f, -1.0f, 0.0f), glm::vec3(0.0f), glm::vec3(0.0f), glm::vec2(1.0f, 0.0f)),
        Vertex(glm::vec3(1.0f,  1.0f, 0.0f), glm::vec3(0.0f), glm::vec3(0.0f), glm::vec2(1.0f, 1.0f)),
        Vertex(glm::vec3(-1.0f,  1.0f, 0.0f), glm::vec3(0.0f), glm::vec3(0.0f), glm::vec2(0.0f, 1.0f))
    };

    std::vector<GLuint> indices = { 0, 1, 2, 0, 2, 3 };

    SceneVBO = std::make_unique<VBO>(vertices);
    SceneEBO = std::make_unique<EBO>(indices);

    SceneVAO.LinkAttrib(*SceneVBO, 0, 3, GL_FLOAT, 11 * sizeof(float), (void*)0);
    SceneVAO.LinkAttrib(*SceneVBO, 1, 3, GL_FLOAT, 11 * sizeof(float), (void*)(3 * sizeof(float)));
    SceneVAO.LinkAttrib(*SceneVBO, 2, 3, GL_FLOAT, 11 * sizeof(float), (void*)(6 * sizeof(float)));
    SceneVAO.LinkAttrib(*SceneVBO, 3, 2, GL_FLOAT, 11 * sizeof(float), (void*)(9 * sizeof(float)));

    SceneVBO->Unbind();
    SceneVAO.Unbind();
    SceneEBO->Unbind();
}

//
// OnWindowClose() – Cleanup resources on window close.
//
void RayScene::OnWindowClose(Window& win) {
    SceneVAO.Delete();
    SceneVBO->Delete();
    SceneEBO->Delete();
    shader.Delete();
    tex.Delete();
}