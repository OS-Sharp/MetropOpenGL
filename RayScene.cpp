#include"RayScene.h";
#include "ComputeStructures.h"
#include <chrono>
#include"Text.h"

    int frameCount = 0;
    float fps = 0.0f;
    std::chrono::time_point<std::chrono::steady_clock> lastTime = std::chrono::steady_clock::now();
    int debugMode = 0;

    RayScene::RayScene(Window& win) :
        SCREEN_WIDTH(win.width),
        SCREEN_HEIGHT(win.height),
        shader("default.vert", "default.frag"),
        computeShader("compute.glsl"),
        tex(SCREEN_WIDTH, SCREEN_HEIGHT, 0,0),
        oldTex(SCREEN_WIDTH, SCREEN_HEIGHT, 1,1),
        averageTex(SCREEN_WIDTH, SCREEN_HEIGHT, 2, 2),
        camera(SCREEN_WIDTH, SCREEN_HEIGHT, glm::vec3(0.0f, 0.0f, -5.0f)),
        model("models/Pipe.obj"), textShader("text_vertex.vert", "text_fragment.frag"),
        text(SCREEN_WIDTH, SCREEN_HEIGHT, "fonts/Raleway-Black.ttf")
    {
        
    }


    void updateFPS() {
        frameCount++;

        // Get the current time
        auto currentTime = std::chrono::steady_clock::now();
        std::chrono::duration<float> elapsed = currentTime - lastTime;

        if (elapsed.count() >= 1.0f) { // Update every 1 second
            fps = frameCount / elapsed.count(); // Calculate FPS
            frameCount = 0; // Reset frame count
            lastTime = currentTime; // Reset time
        }
    }

    void RayScene::AddMeshes() {

        std::vector<Triangle> triangles = {};
        std::vector<MeshInfo> meshes = {};

        Material material;

        material.emmisionColor = glm::vec3(1.0f, 1.0f, 1.0f);
        material.emmisionStrength = glm::vec3(0, 0, 0);
        material.diffuseColor = glm::vec3(0.1f, 0.1f, 0.1f);
        material.specularChance = glm::vec3(0.4f, 0, 0);
        material.smoothness = glm::vec3(1.0f, 0, 0);
        material.opacity = glm::vec3(1.0f, 0, 0);

        model.ToMeshInfo(triangles, meshes, material);


        //THESE WORK TOGETHER
        computeShader.StoreSSBO<MeshInfo>(meshes, 7);
        computeShader.StoreSSBO<Triangle>(triangles, 8);
        computeShader.StoreSSBO<GLuint>(static_cast<GLuint>(meshes.size()), 9);
    }

    void RayScene::AddSurfaces() {
        std::vector<TraceCircle> circles = {};
        const float noOfCircles = 5;
        for (int i = 0; i < noOfCircles; i++) {
            TraceCircle circle;
            Material material;

            material.emmisionColor = glm::vec3(1.0f, 1.0f, 1.0f);
            material.emmisionStrength = (i == floor(noOfCircles / 2)) ? glm::vec3(1, 1, 1) : glm::vec3(0, 0, 0);
            material.diffuseColor = (i == 0) ? glm::vec3(1.0f, 1.0f, 1.0f) : glm::vec3(1.0f, 1 - i / noOfCircles, 1 - i / noOfCircles);
            material.smoothness = glm::vec3(i / noOfCircles,0,0);
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

        computeShader.StoreSSBO<TraceCircle>(circles, 4);
        computeShader.StoreSSBO<GLuint>(static_cast<GLuint>(circles.size()), 5);

        std::vector<TraceDebugBox> boxes = {};

        TraceDebugBox box;
        Material matBox;

        matBox.emmisionColor = glm::vec3(1.0f, 1.0f, 1.0f);
        matBox.emmisionStrength = glm::vec3(0.0f, 0.0f, 0.0f);
        matBox.diffuseColor = glm::vec3(0.9f, 0.4f, 0.4f);
        matBox.specularChance = glm::vec3(0.0f, 0, 0);
        matBox.smoothness = glm::vec3(0.0f, 0, 0);
        matBox.opacity = glm::vec3(0.1f, 0, 0);

        box.material = matBox;
        box.position = glm::vec3(2.0f, 2.0f, 10.0f);
        box.size = glm::vec3(2.0f, 2.0f, 2.0f);
        boxes.push_back(box);

        computeShader.StoreSSBO<TraceDebugBox>(boxes, 10);
        computeShader.StoreSSBO<GLuint>(static_cast<GLuint>(boxes.size()), 11);
    }

    bool wasPressed = false;
    void RayScene::OnBufferSwap(Window& win) {

        if (glfwGetKey(win.instance, GLFW_KEY_B) == GLFW_PRESS && !wasPressed)
        {
            debugMode = 1 - debugMode;
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

        GLuint frame = computeShader.StoreSSBO<GLuint>(Frame, 6);
        Frame++;

        // CAMERA PARAMS
        CameraSettings cameraSettings;
        cameraSettings.position = glm::vec3(camera.Position.x, camera.Position.y, camera.Position.z);
        cameraSettings.direction = glm::vec3(camera.Orientation.x, camera.Orientation.y, camera.Orientation.z);
        cameraSettings.fov = 90.0f;
        GLuint camSettings = computeShader.StoreSSBO<CameraSettings>(cameraSettings, 3);

        AddSurfaces(); // Only update surfaces if necessary
        AddMeshes();
  
        computeShader.Activate();

        computeShader.SetParameterColor(glm::vec3(0.1f, 0.2f, 0.5f), "SkyColourHorizon");
        computeShader.SetParameterColor(glm::vec3(0, 0.1f, 0.3f), "SkyColourZenith");
        computeShader.SetParameterColor(glm::normalize(glm::vec3(1.0f, -0.5f, -1.0f)), "SunLightDirection");
        computeShader.SetParameterColor(glm::vec3(0.1f, 0.1f, 0.1f), "GroundColor");

        computeShader.SetParameterFloat(55.0f, "SunFocus");
        computeShader.SetParameterFloat(2.0f, "SunIntensity");
        computeShader.SetParameterFloat(0.0f, "SunThreshold");
        computeShader.SetParameterInt(debugMode, "DebugMode");

        computeShader.SetParameterInt(4, "NumberOfBounces");
        computeShader.SetParameterInt(3, "NumberOfRays");

        computeShader.Dispatch(SCREEN_WIDTH / 8, SCREEN_HEIGHT / 8, 1);

        tex.texUnit(shader, "tex0", 0);
        oldTex.texUnit(shader, "tex1", 1);
        averageTex.texUnit(shader, "tex2", 2);

        shader.SetParameterInt(Frame, "Frame");

        shader.Activate();
    
        glDrawElements(GL_TRIANGLES, 6, GL_UNSIGNED_INT, 0);

        computeShader.DeleteSSBOs();
        textShader.Activate();
        // Update the FPS counter

        // Display FPS (for example, printing to console)
        text.RenderText(textShader, "fps: " + std::to_string(static_cast<int>(fps)), 25.0f, 25.0f, 1.0f, glm::vec3(1.0f, 1.0f, 1.0f));
    }

    void RayScene::OnWindowLoad(Window& win) {
        //-------------------------------------
        // VERTEX OBJECTS                     
        //-------------------------------------
        SceneVAO.Bind();

        std::cout << model.mesh_list[0].vert_positions[0].b << std::endl;

        std::vector<Vertex> vertices = {
            Vertex(glm::vec3(-1.0f, -1.0f, 0.0f), glm::vec3(0.0f, 0.0f, 0.0f), glm::vec3(0.0f, 0.0f, 0.0f), glm::vec2(0.0f, 0.0f)),
            Vertex(glm::vec3(1.0f, -1.0f, 0.0f), glm::vec3(0.0f, 0.0f, 0.0f), glm::vec3(0.0f, 0.0f, 0.0f), glm::vec2(1.0f, 0.0f)),
            Vertex(glm::vec3(1.0f,  1.0f, 0.0f), glm::vec3(0.0f, 0.0f, 0.0f), glm::vec3(0.0f, 0.0f, 0.0f), glm::vec2(1.0f, 1.0f)),
            Vertex(glm::vec3(-1.0f,  1.0f, 0.0f), glm::vec3(0.0f, 0.0f, 0.0f), glm::vec3(0.0f, 0.0f, 0.0f), glm::vec2(0.0f, 1.0f))
        };

        std::vector<GLuint> indices = {
            0,1,2,
            0,2,3
        };

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
