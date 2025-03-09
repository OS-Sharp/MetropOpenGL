#include "RayScene.h"
#include "ComputeStructures.h"
#include <chrono>
#include "../Core/Text.h"

// Global variables
int frameCount = 0;
float fps = 0.0f;
std::chrono::time_point<std::chrono::steady_clock> lastTime = std::chrono::steady_clock::now();
BVH sceneBVH;

int DEBUGMODE = 0;

const int DEBUGTEST = 0;
const int DEBUGTHRESHOLD = 30;
const int RAYSPERPIXEL = 1;
const int METROPLIS_MUTATIONS = 1;
const int BOUNCES = 3;

const bool METROPLIS_MODE = true;

const int LAYOUT_SIZE_X = 8;
const int LAYOUT_SIZE_Y = 8;

const int METROPLIS_DISPATCH_X = 32;
const int METROPLIS_DISPATCH_Y = 32;

static auto startTime = std::chrono::steady_clock::now();

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
    model("models/BOTTLE MID POLY.obj"),
    textShader("shaders/text_vertex.vert", "shaders/text_fragment.frag"),
    text(SCREEN_WIDTH, SCREEN_HEIGHT, "fonts/Raleway-Black.ttf")
{

    
    Material material;
    material.emmisionColor = glm::vec3(0.0f, 0.0f, 0.0f);
    material.emmisionStrength = glm::vec3(0, 0, 0);
    material.diffuseColor = glm::vec3(0.8f, 0.3f, 0.3f);
    material.specularChance = glm::vec3(0.2f, 0, 0);
    material.smoothness = glm::vec3(0.4f, 0, 0);

    ASSModel dragon("models/FilchCorridor.obj");
    std::vector<Triangle> dragonTris = dragon.ToTriangles(material, 0.001f, glm::vec3(0, 0, 0));
    sceneBVH.AddModel(dragonTris, material);
    

    /*
    Material material;
    material.emmisionColor = glm::vec3(0.0f, 0.0f, 0.0f);
    material.emmisionStrength = glm::vec3(0, 0, 0);
    material.diffuseColor = glm::vec3(1.0f, 1.0f, 1.0f);
    material.specularChance = glm::vec3(0.8f, 0, 0);
    material.smoothness = glm::vec3(1.0f, 0, 0);

    ASSModel dragon("models/Dragon.obj");
    std::vector<Triangle> dragonTris = dragon.ToTriangles(material, 1.0f, glm::vec3(0, 0, 0));
    sceneBVH.AddModel(dragonTris, material);
    */

    /*
    Material material;
    material.emmisionColor = glm::vec3(0.0f, 0.0f, 0.0f);
    material.emmisionStrength = glm::vec3(0, 0, 0);
    material.diffuseColor = glm::vec3(1.0f, 1.0f, 1.0f);
    material.specularChance = glm::vec3(0.2f, 0, 0);
    material.smoothness = glm::vec3(0.2, 0, 0);
    
    ASSModel dragon("models/Chess_Set_Joined1.obj");
    std::vector<Triangle> dragonTris = dragon.ToTriangles(material, 1.0f, glm::vec3(0, 0, 0));
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
    material2.emmisionColor = glm::vec3(1.0f, 1.0f, 1.0f);
    //material2.emmisionStrength = glm::vec3(4.0f, 4.0f, 4.0f);
    material2.emmisionStrength = glm::vec3(31.0f, 31.0f, 31.0f);
    material2.diffuseColor = glm::vec3(1.0f, 1.0f, 1.0f);
    material2.specularChance = glm::vec3(1.0f, 0, 0);
    material2.smoothness = glm::vec3(1.0f, 0, 0);

    circle2.material = material2;
    //circle2.position = glm::vec3(35.0f , 40.0f, 0.0f);
    circle2.position = glm::vec3(0.0f, 10.0f, -60.0f);
    circle2.radius = 12.0f;

    circles.push_back(circle2);

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

bool wasPressed = false;
void RayScene::OnBufferSwap(Window& win) {

    auto currentTimePoint = std::chrono::steady_clock::now();
    std::chrono::duration<float> elapsedSeconds = currentTimePoint - startTime;
    float timeInSeconds = elapsedSeconds.count();

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

    if (hasMoved)
    {
        Frame = 0;
        GLuint clearValue = 0;

        // Define the clear color as a vec4 with all zeros
        GLfloat clearColor[4] = { 0.0f, 0.0f, 0.0f, 0.0f };
        GLfloat whiteClear[4] = { 1.0f, 1.0f, 1.0f, 1.0f };

        // Clear the texture at mipmap level 0 using the format GL_RGBA and type GL_FLOAT.
        glClearTexImage(tex.ID, 0, GL_RGBA, GL_FLOAT, clearColor);
        glClearTexImage(oldTex.ID, 0, GL_RGBA, GL_FLOAT, clearColor);
        glClearTexImage(biasTex.ID, 0, GL_RGBA, GL_FLOAT, clearColor);
        glClearTexImage(metroplisColorsTex.ID, 0, GL_RGBA, GL_FLOAT, clearColor);
        glClearTexImage(metroplisDirectionsTex.ID, 0, GL_RGBA, GL_FLOAT, clearColor);

        //glClearTexImage(averageTex.ID, 0, GL_RGBA, GL_FLOAT, clearColor);
    }

    glMemoryBarrier(GL_ALL_BARRIER_BITS);

    glCopyImageSubData(tex.ID, GL_TEXTURE_2D, 0, 0, 0, 0,
        oldTex.ID, GL_TEXTURE_2D, 0, 0, 0, 0,
        SCREEN_WIDTH, SCREEN_HEIGHT, 1);
    // Update global frame data.
    // (Frame now lives at binding 4, not 6.)
    GLuint frame = computeShader.StoreSSBO<GLuint>(Frame, 4);
    Frame++;

    // CAMERA PARAMS – CameraSettings are uploaded to binding 3 (unchanged)
    CameraSettings cameraSettings;
    cameraSettings.position = glm::vec3(camera.Position.x, camera.Position.y, camera.Position.z);
    cameraSettings.direction = glm::vec3(camera.Orientation.x, camera.Orientation.y, camera.Orientation.z);
    cameraSettings.fov = 90.0f;
    GLuint camSettings = computeShader.StoreSSBO<CameraSettings>(cameraSettings, 3);

    computeShader.Activate();

    computeShader.SetParameterFloat(timeInSeconds, "uTime");

    computeShader.SetParameterColor(glm::vec3(1.0f, 1.0f, 1.0f), "SkyColourHorizon");
    computeShader.SetParameterColor(glm::vec3(0.08f, 0.37f, 0.73f), "SkyColourZenith");
    computeShader.SetParameterColor(glm::normalize(glm::vec3(1.0f, -0.5f, -1.0f)), "SunLightDirection");
    computeShader.SetParameterColor(glm::vec3(0.35f, 0.3f, 0.35f), "GroundColor");


    computeShader.SetParameterFloat(500.0f, "SunFocus");
    computeShader.SetParameterFloat(10.0f, "SunIntensity");
    computeShader.SetParameterFloat(0.0f, "SunThreshold");
    computeShader.SetParameterInt(DEBUGMODE, "DebugMode");

    computeShader.SetParameterInt(BOUNCES, "NumberOfBounces");
    computeShader.SetParameterInt(RAYSPERPIXEL, "NumberOfRays");
    computeShader.SetParameterInt(METROPLIS_MUTATIONS, "NumberOfMutations");
    computeShader.SetParameterInt(DEBUGTHRESHOLD, "DebugThreshold");
    computeShader.SetParameterInt(DEBUGTEST, "DebugTest");
    computeShader.SetParameterInt(METROPLIS_MODE, "METROPLIS");

    computeShader.SetParameterInt(METROPLIS_DISPATCH_X, "METROPLIS_DISPATCH_X");
    computeShader.SetParameterInt(METROPLIS_DISPATCH_Y, "METROPLIS_DISPATCH_Y");

    glMemoryBarrier(GL_ALL_BARRIER_BITS);

    int gX = METROPLIS_MODE ? METROPLIS_DISPATCH_X : SCREEN_WIDTH / LAYOUT_SIZE_X;
    int gY = METROPLIS_MODE ? METROPLIS_DISPATCH_Y : SCREEN_HEIGHT / LAYOUT_SIZE_Y;
    int gZ = 1;

    computeShader.Dispatch(gX, gY, gZ);
    glMemoryBarrier(GL_ALL_BARRIER_BITS);

    tex.texUnit(shader, "tex0", 0);
    biasTex.texUnit(shader, "tex1", 1);
    oldTex.texUnit(shader, "tex2", 2);

    shader.SetParameterInt(Frame, "Frame");

    shader.Activate();
    glDrawElements(GL_TRIANGLES, 6, GL_UNSIGNED_INT, 0);

    computeShader.DeleteSSBOs();
    textShader.Activate();

    // Display FPS
    text.RenderText(textShader, "fps: " + std::to_string(static_cast<int>(fps)),
        25.0f, 25.0f, 1.0f, glm::vec3(1.0f, 1.0f, 1.0f));
}

void RayScene::OnWindowLoad(Window& win) {
    // VERTEX OBJECTS
    SceneVAO.Bind();

    std::cout << model.mesh_list[0].vert_positions[0].b << std::endl;

    std::vector<Vertex> vertices = {
        Vertex(glm::vec3(-1.0f, -1.0f, 0.0f), glm::vec3(0.0f), glm::vec3(0.0f), glm::vec2(0.0f)),
        Vertex(glm::vec3(1.0f, -1.0f, 0.0f), glm::vec3(0.0f), glm::vec3(0.0f), glm::vec2(1.0f, 0.0f)),
        Vertex(glm::vec3(1.0f,  1.0f, 0.0f), glm::vec3(0.0f), glm::vec3(0.0f), glm::vec2(1.0f, 1.0f)),
        Vertex(glm::vec3(-1.0f,  1.0f, 0.0f), glm::vec3(0.0f), glm::vec3(0.0f), glm::vec2(0.0f, 1.0f))
    };

    std::vector<GLuint> indices = { 0, 1, 2, 0, 2, 3 };

    // Initialize SceneVBO and SceneEBO using pointers
    SceneVBO = std::make_unique<VBO>(vertices);
    SceneEBO = std::make_unique<EBO>(indices);

    SceneVAO.LinkAttrib(*SceneVBO, 0, 3, GL_FLOAT, 11 * sizeof(float), (void*)0);
    SceneVAO.LinkAttrib(*SceneVBO, 1, 3, GL_FLOAT, 11 * sizeof(float), (void*)(3 * sizeof(float)));
    SceneVAO.LinkAttrib(*SceneVBO, 2, 3, GL_FLOAT, 11 * sizeof(float), (void*)(6 * sizeof(float)));
    SceneVAO.LinkAttrib(*SceneVBO, 3, 2, GL_FLOAT, 11 * sizeof(float), (void*)(9 * sizeof(float)));

    // Unbind
    SceneVBO->Unbind();
    SceneVAO.Unbind();
    SceneEBO->Unbind();
}

void RayScene::OnWindowClose(Window& win) {
    SceneVAO.Delete();
    SceneVBO->Delete();
    SceneEBO->Delete();
    shader.Delete();
    tex.Delete();
}
