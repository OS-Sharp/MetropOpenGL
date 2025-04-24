#include "RayScene.h"
#include "ComputeStructures.h"
#include <chrono>
#include "../Core/Text.h"
#include "../../stb_image_write.h"

#ifdef _WIN32
#include <windows.h> // Include Windows API headers for CreateDirectory
#include <direct.h>  // For _mkdir
#else
#include <sys/stat.h> // For mkdir on Unix systems
#endif

// Global variables for frame timing and debug
double frameCount = 0;
float fps = 0.0f;
std::chrono::time_point<std::chrono::steady_clock> lastTime = std::chrono::steady_clock::now();
BVH sceneBVH;

int DEBUGMODE = 0;
float SKYSTRENGTH = 0.14f;

const int DEBUGTEST = 1;
const int DEBUGTHRESHOLD = 30;
const int RAYSPERPIXEL = 1;
const int METROPLIS_MUTATIONS = 1;
const int BOUNCES = 12;
bool wasPressed = false;

// Render mode enumeration
enum RenderMode {
    PATH_TRACING = 0,
    METROPLIS = 1,
    PATH_TRACING_BIDIRECTIONAL = 2
};

enum ScenePreset {
    CornellBox,
    TableAndChairs,
    ChessCaustics,
    IndoorDiffuse,
};

const RenderMode renderMode = PATH_TRACING_BIDIRECTIONAL;
const ScenePreset preset = ChessCaustics;

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
    const int localSizeX = 16;  // Must match shader's local_size_x
    const int localSizeY = 16;  // Must match shader's local_size_y

    int totalThreads = dispatchX * dispatchY * localSizeX * localSizeY;
    int bufferSize = MAX_STACK_SIZE * totalThreads * sizeof(int);

    GLuint bvhStackBuffer;
    glGenBuffers(1, &bvhStackBuffer);
    glBindBuffer(GL_SHADER_STORAGE_BUFFER, bvhStackBuffer);
    glBufferData(GL_SHADER_STORAGE_BUFFER, bufferSize, nullptr, GL_DYNAMIC_COPY);
    glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 20, bvhStackBuffer);
    glBindBuffer(GL_SHADER_STORAGE_BUFFER, 0);


    std::vector<Triangle> tris;
    glm::vec3 camPos, camOri;
    float scale = 1.0f;
    glm::vec3 translate(0.0f);
    float skyStrength = SKYSTRENGTH;

    switch (preset) {
    case ScenePreset::CornellBox: {
        Material mat;
        mat.emmisionColor = glm::vec3(0.0f);
        mat.emmisionStrength = 0.0f;
        mat.diffuseColor = glm::vec3(1.0f);          // slightly gray walls
        mat.specularChance = 0.2f;
        mat.smoothness = 1.0f;

        ASSModel model = ASSModel("models/CornellBox-Original.obj");
        scale = 10.0f;
        translate = glm::vec3(0, -60, 0);
        camPos = glm::vec3(-0.64f, -48.49f, 20.38f);
        camOri = glm::vec3(-0.01f, -0.07f, -1.00f);
        skyStrength = 0.1f;

        model.ToTriangles(mat, scale, translate, computeShader, sceneBVH, false, false);
        break;
    }

    case ScenePreset::TableAndChairs: {
        Material mat;
        mat.emmisionColor = glm::vec3(0.0f);
        mat.emmisionStrength = 0.0f;
        mat.diffuseColor = glm::vec3(0.6f, 0.4f, 0.2f); // wood tone
        mat.specularChance = 0.0f;
        mat.smoothness = 0.0f;

        ASSModel model = ASSModel("models/Table And Chairs.obj");
        scale = 1.0f;
        translate = glm::vec3(0, -23, 0);
        camPos = glm::vec3(-10.22f, 44.09f, -20.94f);
        camOri = glm::vec3(0.58f, -0.81f, 0.03f);
        skyStrength = 0.1f;

        Material m2;
        m2.emmisionColor = glm::vec3(0.0f, 0.0f, 0.0f);
        m2.emmisionStrength = 0;
        m2.diffuseColor = glm::vec3(0.91f, 0.98f, 0.98f );
        m2.specularChance = 0.2f;
        m2.smoothness = 0.2f;
        m2.isTranslucent = 1.0f;
        m2.refractiveIndex = 1.4404f;

        ASSModel plane("models/uploads_files_3034691_Absinthium_Glass.obj");

        plane.ToTriangles(m2, 200.0f, glm::vec3(-14.0f, 19.1f, -18.0f), computeShader, sceneBVH, true);
        tris = model.ToTriangles(mat, scale, translate, computeShader, sceneBVH);
        break;
    }

    case ScenePreset::ChessCaustics: {
        Material m1;
        m1.emmisionColor = glm::vec3(0.0f, 0.0f, 0.0f);
        m1.emmisionStrength = 0;
        m1.diffuseColor = glm::vec3(0.9f, 0.9f, 0.9f);
        m1.specularChance = 1.0f;
        m1.smoothness = 1.0f;
        m1.isTranslucent = 1.0;
        m1.refractiveIndex = 1.57f;

        ASSModel set("models/Chess_Set_Joined1.obj");
        set.ToTriangles(m1, 1.0f, glm::vec3(0, 0, 0), computeShader, sceneBVH, true);

        Material m2;
        m2.emmisionColor = glm::vec3(0.0f, 0.0f, 0.0f);
        m2.emmisionStrength = 0;
        m2.diffuseColor = glm::vec3(1.0f, 1.0f, 1.0f);
        m2.specularChance = 0.0f;
        m2.smoothness = 0.0f;


        ASSModel plane("models/chess/chess.obj");
        plane.ToTriangles(m2, 56.0f, glm::vec3(130.f, 7.0f, -130.0f), computeShader, sceneBVH, true);
        camPos = glm::vec3(59.32f, 38.00f, -105.32f);
        camOri = glm::vec3(-0.54f, -0.28f, 0.80f);
        skyStrength = 0.25f;
        break;
    }

    case ScenePreset::IndoorDiffuse:
    default: {
        Material mat;
        mat.emmisionColor = glm::vec3(0.0f);
        mat.emmisionStrength = 0;
        mat.diffuseColor = glm::vec3(1.0f);          // pure white
        mat.specularChance = 0;
        mat.smoothness = 0;

        ASSModel model = ASSModel("models/hall01.obj");
        scale = 5.0f;
        translate = glm::vec3(0, 22, 0);
        camPos = glm::vec3(21.04f, 28.01f, -3.88f);
        camOri = glm::vec3(-0.9f, -0.11f, 0.42f);
        skyStrength = 0.82f;

        tris = model.ToTriangles(mat, scale, translate, computeShader, sceneBVH);
        break;
    }
    }

    // Finally, apply camera/sky
    SKYSTRENGTH = skyStrength;
    camera.Position = camPos;
    camera.Orientation = camOri;


    AddMeshes();
    AddSurfaces();

    if (renderMode == PATH_TRACING_BIDIRECTIONAL) {

        // Define constants for path tracing
        const int MAX_PATH_LENGTH = 6;  // Must match your shader definition

        // Create properly sized vertex buffers for paths
        // Define a dummy Vertex aligned with your shader's Vertex structure
        struct ShaderVertex {
            glm::vec3 position;       // 12 bytes
            int objIndex;
            glm::vec3 normal;         // 12 bytes
            int objType;
            glm::vec3 throughput;     // 12 bytes
            float PDF;                // 4 bytes
            glm::vec3 emmision;
        };

        // Calculate total needed size
        size_t vertexBufferSize = sizeof(ShaderVertex) * MAX_PATH_LENGTH * totalThreads;

        // Create the camera path buffer using your existing Shader class methods
        // We create empty buffers sized appropriately for the data
        std::vector<unsigned char> cameraPathData(vertexBufferSize, 0);
        computeShader.StoreSSBO(cameraPathData, 21, false);

        // Create the light path buffer
        std::vector<unsigned char> lightPathData(vertexBufferSize, 0);
        computeShader.StoreSSBO(lightPathData, 22, false);


    }
}


// Add this method to your RayScene class
void RayScene::SetupEmissiveObjectsBuffer(const std::vector<TraceCircle> circles) {
    std::vector<EmissiveObjectData> emissiveObjects;
    float totalPower = 0.0f;

    // First gather all emissive spheres
    for (size_t i = 0; i < circles.size(); i++) {
        const TraceCircle& sphere = circles[i];
        if (glm::length(sphere.material.emmisionStrength) > 0.001f) {
            EmissiveObjectData obj;
            obj.position = sphere.position;
            obj.radius = sphere.radius;
            obj.normal = glm::vec3(0.0f); // Not used for spheres
            obj.type = 0.0f; // 0 = sphere
            obj.objectIndex = static_cast<int>(i);

            // Calculate total emissive power (approximate)
            float avgEmission = (sphere.material.emmisionColor.r +
                sphere.material.emmisionColor.g +
                sphere.material.emmisionColor.b) / 3.0f;
            float intensity = sphere.material.emmisionStrength;
            float surfaceArea = 4.0f * glm::pi<float>() * sphere.radius * sphere.radius;

            obj.power = avgEmission * intensity * surfaceArea;
            obj.emission = sphere.material.emmisionColor * sphere.material.emmisionStrength;
            obj.padding = 0.0f;

            totalPower += obj.power;
            emissiveObjects.push_back(obj);
        }
    }

    // Then gather all emissive triangles
    for (size_t i = 0; i < sceneBVH.Triangles.size(); i++) {
        const Triangle& tri = sceneBVH.Triangles[i];
        int meshIndex = -1;

        // Find which model this triangle belongs to
        for (size_t j = 0; j < sceneBVH.Models.size(); j++) {
            const BVHModel& model = sceneBVH.Models[j];
            int triStart = model.TriangleOffset;
            int triEnd = triStart + sceneBVH.FlatNodes[model.NodeOffset].TriangleCount;

            if (i >= triStart && i < triEnd) {
                meshIndex = static_cast<int>(j);
                break;
            }
        }

        if (meshIndex >= 0) {
            const Material& material = sceneBVH.Models[meshIndex].material;
            if (glm::length(material.emmisionStrength) > 0.001f) {
                EmissiveObjectData obj;

                // Calculate triangle barycenter and area
                obj.position = (tri.P1 + tri.P2 + tri.P3) / 3.0f;

                // Calculate triangle area
                glm::vec3 edge1 = tri.P2 - tri.P1;
                glm::vec3 edge2 = tri.P3 - tri.P1;
                glm::vec3 normal = glm::normalize(glm::cross(edge1, edge2));
                float area = 0.5f * glm::length(glm::cross(edge1, edge2));

                obj.radius = area; // Store area in radius field
                obj.normal = normal;
                obj.type = 1.0f; // 1 = triangle
                obj.objectIndex = static_cast<int>(i);

                // Calculate total emissive power (approximate)
                float avgEmission = (material.emmisionColor.r +
                    material.emmisionColor.g +
                    material.emmisionColor.b) / 3.0f;
                float intensity = material.emmisionStrength;

                obj.power = avgEmission * intensity * area;
                obj.emission = material.emmisionColor * material.emmisionStrength;
                obj.padding = 0.0f;

                totalPower += obj.power;
                emissiveObjects.push_back(obj);
            }
        }
    }

    // Create emissive objects buffer
    if (!emissiveObjects.empty()) {
        computeShader.StoreSSBO<EmissiveObjectData>(emissiveObjects, 23, false);

        // Create power info buffer
        EmissivePowerInfo powerInfo;
        powerInfo.totalEmissivePower = totalPower;
        powerInfo.numEmissiveObjects = static_cast<int>(emissiveObjects.size());
        powerInfo.padding = glm::vec2(0.0f);

        computeShader.StoreSSBO<EmissivePowerInfo>(powerInfo, 24, false);

        std::cout << "Created emissive objects buffer with " << emissiveObjects.size()
            << " objects, total power: " << totalPower << std::endl;
    }
    else {
        // Create empty buffers with zero objects
        EmissivePowerInfo powerInfo = { 0.0f, 0, glm::vec2(0.0f) };
        computeShader.StoreSSBO<EmissivePowerInfo>(powerInfo, 24, false);

        // Create a dummy empty buffer for emissive objects
        std::vector<EmissiveObjectData> dummyBuffer(1); // One empty entry
        computeShader.StoreSSBO<EmissiveObjectData>(dummyBuffer, 23, false);

        std::cout << "No emissive objects found in scene" << std::endl;
    }
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
    material2.emmisionStrength = 10.0f;
    material2.diffuseColor = glm::vec3(1.0f, 1.0f, 1.0f);
    material2.specularChance = 0;
    material2.smoothness = 0;

    circle2.material = material2;
    //circle2.position = glm::vec3(-14.0f, 25.1f, -18.0f);
    circle2.position = glm::vec3(0.0f, 50.0f, -55.0f);

    circle2.radius = 6.5f;

    circles.push_back(circle2);

    TraceCircle circle3;

    Material materialbrown;
    materialbrown.emmisionColor = glm::vec3(0.69f, 0.682f, 0.271f);
    //material2.emmisionStrength = glm::vec3(4.0f, 4.0f, 4.0f);
    materialbrown.emmisionStrength = 1.0f * 0.5f;
    materialbrown.diffuseColor = glm::vec3(0.69f, 0.682f, 0.271f);
    materialbrown.specularChance = 1.0f;
    materialbrown.smoothness = 1.0f;

    circle3.material = materialbrown;
    //circle2.position = glm::vec3(35.0f , 40.0f, 0.0f);
    circle3.position = glm::vec3(-40.0f, 7.0f, -55.0f);
    circle3.radius = 1.4f;

    circles.push_back(circle3);

    TraceCircle circleFar;
    circleFar.material = material2;
    circleFar.position = glm::vec3(5.0f, 50.0f, 5.0f);
    //circleFar.position = glm::vec3(5.0f, 00.0f, 20.0f);
    circleFar.radius = 12.0f;

    circles.push_back(circleFar);

    TraceCircle reflective2;
    Material material4;
    material4.emmisionColor = glm::vec3(1.0f, 1.0f, 1.0f);
    material4.emmisionStrength = 0;
    material4.diffuseColor = glm::vec3(0.3f, 1.0f, 0.3f);
    material4.specularChance = 0.9f;
    material4.smoothness = 1.0f;
    material4.isTranslucent = 1;
    material4.refractiveIndex = 1.57f;

    reflective2.material = material4;
    reflective2.position = glm::vec3(0, 25.0f, 0.0f);
    reflective2.radius = 5.0f;

    circles.push_back(reflective2);


    // Upload circles to binding 5 and their count to binding 6.
    computeShader.StoreSSBO<TraceCircle>(circles, 7, false);
    computeShader.StoreSSBO<GLuint>(static_cast<GLuint>(circles.size()), 8, false);

    SetupEmissiveObjectsBuffer(circles);
}


// Add this function to save screenshots
bool RayScene::SaveScreenshot(double timeInSeconds) {
    // Create buffer to store the pixels
    std::vector<unsigned char> pixels(SCREEN_WIDTH * SCREEN_HEIGHT * 4);

    // Read pixels from the framebuffer
    glReadPixels(0, 0, SCREEN_WIDTH, SCREEN_HEIGHT, GL_RGBA, GL_UNSIGNED_BYTE, pixels.data());

    // Flip the image vertically since OpenGL's coordinate system is different
    std::vector<unsigned char> flippedPixels(SCREEN_WIDTH * SCREEN_HEIGHT * 4);
    for (int y = 0; y < SCREEN_HEIGHT; y++) {
        for (int x = 0; x < SCREEN_WIDTH; x++) {
            for (int c = 0; c < 4; c++) {
                flippedPixels[(y * SCREEN_WIDTH + x) * 4 + c] =
                    pixels[((SCREEN_HEIGHT - 1 - y) * SCREEN_WIDTH + x) * 4 + c];
            }
        }
    }

    // Get scene name from the enum
    std::string sceneName;
    switch (preset) {
    case ScenePreset::CornellBox:
        sceneName = "CornellBox";
        break;
    case ScenePreset::TableAndChairs:
        sceneName = "TableAndChairs";
        break;
    case ScenePreset::ChessCaustics:
        sceneName = "ChessCaustics";
        break;
    case ScenePreset::IndoorDiffuse:
        sceneName = "IndoorDiffuse";
        break;
    default:
        sceneName = "Unknown";
        break;
    }

    // Get render technique from the enum
    std::string renderTechnique;
    switch (renderMode) {
    case RenderMode::PATH_TRACING:
        renderTechnique = "PathTracing";
        break;
    case RenderMode::METROPLIS:
        renderTechnique = "Metropolis";
        break;
    case RenderMode::PATH_TRACING_BIDIRECTIONAL:
        renderTechnique = "BiPathTracing";
        break;
    default:
        renderTechnique = "Unknown";
        break;
    }

    // Calculate SPP (samples per pixel)
    int spp = Frame * RAYSPERPIXEL;

    // Create directory structure if it doesn't exist
    std::string parentDir = "renders";
    std::string dirPath = parentDir + "/" + sceneName + "/" + renderTechnique;

#ifdef _WIN32
    // Windows directory creation using _mkdir
    // First create renders directory if it doesn't exist
    int result = _mkdir(parentDir.c_str());
    if (result != 0 && errno != EEXIST) {
        std::cerr << "Failed to create parent directory: " << parentDir << std::endl;
        return false;
    }

    // Then create the scene-specific directory
    result = _mkdir(dirPath.c_str());
    if (result != 0 && errno != EEXIST) {
        std::cerr << "Failed to create directory: " << dirPath << std::endl;
        return false;
    }
#else
    // Unix directory creation
    // Create parent directory
    int result = mkdir(parentDir.c_str(), 0755);
    if (result != 0 && errno != EEXIST) {
        std::cerr << "Failed to create parent directory: " << parentDir << std::endl;
        return false;
    }

    // Create scene directory
    result = mkdir(dirPath.c_str(), 0755);
    if (result != 0 && errno != EEXIST) {
        std::cerr << "Failed to create directory: " << dirPath << std::endl;
        return false;
    }
#endif

    // Build the complete filename with path
    std::ostringstream filename;
    filename << dirPath << "/"
        << sceneName << "_"
        << std::fixed << std::setprecision(1) << timeInSeconds << "s_"
        << renderTechnique << "_"
        << spp << "spp"
        << ".png";

    // Save the image using stb_image_write
    int success = stbi_write_png(
        filename.str().c_str(),
        SCREEN_WIDTH,
        SCREEN_HEIGHT,
        4,  // 4 components (RGBA)
        flippedPixels.data(),
        SCREEN_WIDTH * 4
    );

    if (success) {
        std::cout << "Screenshot saved: " << filename.str() << std::endl;
        return true;
    }
    else {
        std::cerr << "Failed to save screenshot!" << std::endl;
        return false;
    }
}
//
// OnBufferSwap() – Called on each buffer swap to update frame data, dispatch the compute shader,
// copy accumulated image data, update camera settings, and render the final output.
//
void RayScene::OnBufferSwap(Window& win) {
    auto currentTimePoint = std::chrono::steady_clock::now();
    std::chrono::duration<float> elapsedSeconds = currentTimePoint - startTime;
    double timeInSeconds = elapsedSeconds.count();

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
    double frame = computeShader.StoreSSBO<double>(Frame, 4);
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
    computeShader.SetParameterFloat(SKYSTRENGTH, "SkyStrength");
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
    tex.texUnit(shader, "tex0");
    biasTex.texUnit(shader, "tex1");
    oldTex.texUnit(shader, "tex2");

    //tex.texUnit(shader, "diffuseTextures");

    shader.SetParameterInt(Frame, "Frame");
    shader.Activate();

    glDrawElements(GL_TRIANGLES, 6, GL_UNSIGNED_INT, 0);

    computeShader.DeleteSSBOs();

    // ‑‑ build one single string ------------------------------------------
    std::ostringstream ss;
    ss << "fps : " << static_cast<int>(fps)
        << "   pos : (" << std::fixed << std::setprecision(2)
        << camera.Position.x << ", "
        << camera.Position.y << ", "
        << camera.Position.z << ")   dir : ("
        << camera.Orientation.x << ", "
        << camera.Orientation.y << ", "
        << camera.Orientation.z << ")";

    textShader.Activate();
    // Render FPS text on the screen
    text.RenderText(textShader, ss.str(), 25.0f, 25.0f, 1.0f, glm::vec3(1.0f));

    auto elapsedSinceLastScreenshot = std::chrono::duration_cast<std::chrono::seconds>(
        currentTimePoint - lastScreenshotTime).count();
    if (elapsedSinceLastScreenshot >= ScreenshotFrequency) {  // 60 seconds = 1 minute
        if (SaveScreenshot(timeInSeconds)) {
            lastScreenshotTime = currentTimePoint;  // Update the time only if save was successful
        }
    }
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