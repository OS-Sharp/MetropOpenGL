#include "RayScene.h"
#include "ComputeStructures.h"
#include <chrono>
#include "Text.h"

// Global variables
int frameCount = 0;
float fps = 0.0f;
std::chrono::time_point<std::chrono::steady_clock> lastTime = std::chrono::steady_clock::now();
BVH sceneBVH;

int DEBUGMODE = 0;
int DEBUGTEST = 0;
int DEBUGTHRESHOLD = 30;
int RAYSPERPIXEL = 1;
int BOUNCES = 2;

RayScene::RayScene(Window& win) :
    SCREEN_WIDTH(win.width),
    SCREEN_HEIGHT(win.height),
    shader("default.vert", "default.frag"),
    computeShader("compute.glsl"),
    tex(SCREEN_WIDTH, SCREEN_HEIGHT, 0, 0),
    oldTex(SCREEN_WIDTH, SCREEN_HEIGHT, 1, 1),
    averageTex(SCREEN_WIDTH, SCREEN_HEIGHT, 2, 2),
    camera(SCREEN_WIDTH, SCREEN_HEIGHT, glm::vec3(0.0f, 0.0f, -5.0f)),
    model("models/Bob-omb Battlefield.obj"),
    textShader("text_vertex.vert", "text_fragment.frag"),
    text(SCREEN_WIDTH, SCREEN_HEIGHT, "fonts/Raleway-Black.ttf")
{
    Material material;
    material.emmisionColor = glm::vec3(1.0f, 1.0f, 1.0f);
    material.emmisionStrength = glm::vec3(0, 0, 0);
    material.diffuseColor = glm::vec3(0.1f, 0.1f, 0.1f);
    material.specularChance = glm::vec3(0.4f, 0, 0);
    material.smoothness = glm::vec3(1.0f, 0, 0);
    material.opacity = glm::vec3(1.0f, 0, 0);

    std::vector<Triangle> modelTris = model.ToTriangles(material, 1, glm::vec3(1, 1, 1));
    sceneBVH.AddModel(modelTris, material);
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
    computeShader.StoreSSBO<BVHTriangle>(sceneBVH.Triangles, 9);
    // (If you no longer need to upload a separate triangle count, omit it.)

    // Upload BVH node array to binding 11 and its count to binding 12.
    computeShader.StoreSSBO<BVHNode>(sceneBVH.FlatNodes, 11);
    computeShader.StoreSSBO<GLuint>(static_cast<GLuint>(sceneBVH.FlatNodes.size()), 12);

    // Upload model array to binding 13 and its count to binding 14.
    computeShader.StoreSSBO<BVHModel>(sceneBVH.Models, 13);
    computeShader.StoreSSBO<GLuint>(static_cast<GLuint>(sceneBVH.Models.size()), 14);
}

//
// Updated AddSurfaces() – the circles (spheres) and boxes are now uploaded
// to the new bindings (5–6 for circles; 7–8 for boxes)
//
void RayScene::AddSurfaces() {
    std::vector<TraceCircle> circles;
    const float noOfCircles = 5;
    for (int i = 0; i < noOfCircles; i++) {
        TraceCircle circle;
        Material material;
        material.emmisionColor = glm::vec3(1.0f, 1.0f, 1.0f);
        material.emmisionStrength = (i == floor(noOfCircles / 2)) ? glm::vec3(1, 1, 1) : glm::vec3(0, 0, 0);
        material.diffuseColor = (i == 0) ? glm::vec3(1.0f, 1.0f, 1.0f) : glm::vec3(1.0f, 1 - i / noOfCircles, 1 - i / noOfCircles);
        material.smoothness = glm::vec3(i / noOfCircles, 0, 0);
        material.specularChance = glm::vec3((i * 0.2f) / noOfCircles, 0, 0);
        material.specularColor = material.diffuseColor;
        circle.material = material;
        circle.position = (i == floor(noOfCircles / 2)) ?
            glm::vec3(i * 2.4f - noOfCircles * 0.5f * 2.4f, 1.0f, 7.0f) :
            glm::vec3(i * 2.4f - noOfCircles * 0.5f * 2.4f, 0, 7.0f);
        circle.radius = 1.0f;
        circles.push_back(circle);
    }

    TraceCircle circle;
    Material material;
    material.emmisionColor = glm::vec3(1.0f, 1.0f, 1.0f);
    material.emmisionStrength = glm::vec3(0, 0, 0);
    material.diffuseColor = glm::vec3(0.0f, 0.0f, 1.0f);
    material.specularChance = glm::vec3(0.2f, 0, 0);
    material.smoothness = glm::vec3(0.7, 0, 0);
    circle.material = material;
    circle.position = glm::vec3(0, -10.0f, 11.0f);
    circle.radius = 10.0f;
    circles.push_back(circle);

    TraceCircle circle2;
    Material material2;
    material2.emmisionColor = glm::vec3(1.0f, 1.0f, 1.0f);
    material2.emmisionStrength = glm::vec3(10.0f, 10.0f, 10.0f);
    material2.diffuseColor = glm::vec3(1.0f, 1.0f, 1.0f);
    circle2.material = material2;
    circle2.position = glm::vec3(0, 10.0f, 30.0f);
    circle2.radius = 7.0f;
    circles.push_back(circle2);

    // Upload circles to binding 5 and their count to binding 6.
    computeShader.StoreSSBO<TraceCircle>(circles, 5);
    computeShader.StoreSSBO<GLuint>(static_cast<GLuint>(circles.size()), 6);

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
    boxes.push_back(box);

    // Upload boxes to binding 7 and their count to binding 8.
    computeShader.StoreSSBO<TraceDebugBox>(boxes, 7);
    computeShader.StoreSSBO<GLuint>(static_cast<GLuint>(boxes.size()), 8);
}

bool wasPressed = false;
void RayScene::OnBufferSwap(Window& win) {
    if (glfwGetKey(win.instance, GLFW_KEY_B) == GLFW_PRESS && !wasPressed) {
        DEBUGMODE = 1 - DEBUGMODE;
        std::cout << 1;
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

    if (hasMoved) Frame = 0;

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

    AddSurfaces(); // Upload surfaces (circles and boxes)
    AddMeshes();   // Upload BVH and model data

    computeShader.Activate();

    computeShader.SetParameterColor(glm::vec3(0.1f, 0.2f, 0.5f), "SkyColourHorizon");
    computeShader.SetParameterColor(glm::vec3(0, 0.1f, 0.3f), "SkyColourZenith");
    computeShader.SetParameterColor(glm::normalize(glm::vec3(1.0f, -0.5f, -1.0f)), "SunLightDirection");
    computeShader.SetParameterColor(glm::vec3(0.1f, 0.1f, 0.1f), "GroundColor");

    computeShader.SetParameterFloat(55.0f, "SunFocus");
    computeShader.SetParameterFloat(2.0f, "SunIntensity");
    computeShader.SetParameterFloat(0.0f, "SunThreshold");
    computeShader.SetParameterInt(DEBUGMODE, "DebugMode");

    computeShader.SetParameterInt(BOUNCES, "NumberOfBounces");
    computeShader.SetParameterInt(RAYSPERPIXEL, "NumberOfRays");
    computeShader.SetParameterInt(DEBUGTHRESHOLD, "DebugThreshold");
    computeShader.SetParameterInt(DEBUGTEST, "DebugTest");

    computeShader.Dispatch(SCREEN_WIDTH / 8, SCREEN_HEIGHT / 8, 1);

    tex.texUnit(shader, "tex0", 0);
    oldTex.texUnit(shader, "tex1", 1);
    averageTex.texUnit(shader, "tex2", 2);

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
