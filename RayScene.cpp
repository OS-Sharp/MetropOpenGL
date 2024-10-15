#include"RayScene.h";
#include "ComputeStructures.h"

    RayScene::RayScene(Window& win) :
        SCREEN_WIDTH(win.width),
        SCREEN_HEIGHT(win.height),
        shader("default.vert", "default.frag"),
        computeShader("compute.glsl"),
        tex(SCREEN_WIDTH, SCREEN_HEIGHT, 0,0),
        oldTex(SCREEN_WIDTH, SCREEN_HEIGHT, 1,1),
        averageTex(SCREEN_WIDTH, SCREEN_HEIGHT, 2, 2),
        camera(SCREEN_WIDTH, SCREEN_HEIGHT, glm::vec3(0.0f, 0.0f, -5.0f)),
        model("models/pipe.obj") {}

    void RayScene::AddMeshes() {

        std::vector<Triangle> triangles = {};
        std::vector<MeshInfo> meshes = {};


            for (int a = 0; a < model.mesh_list.size(); a++) {
                auto mesh = model.mesh_list[a];
                std::vector<Triangle> meshTriangles = {};

                std::vector<glm::vec3> positions = {};
                std::vector<glm::vec3> normals = {};

                for (int b = 0; b < mesh.vert_indices.size(); b++) {
                    positions.push_back(mesh.vert_positions[mesh.vert_indices[b]] * 10.0f + glm::vec3(0,0,16));
                    normals.push_back(mesh.vert_normals[mesh.vert_indices[b]]);
                    if ((b + 1) % 3 == 0) {
                        Triangle triangle;

                        triangle.P1 = positions[0];
                        triangle.P2 = positions[1];
                        triangle.P3 = positions[2];

                        triangle.NormP1 = normals[0];
                        triangle.NormP2 = normals[1];
                        triangle.NormP3 = normals[2];

                        meshTriangles.push_back(triangle);

                        positions.clear();
                        normals.clear();
                    }
                }
                
                MeshInfo meshInfo = MeshInfo::createMeshFromTris(triangles.size(), meshTriangles);
                Material material;

                material.emmisionColor = glm::vec3(1.0f, 1.0f, 1.0f);
                material.emmisionStrength = glm::vec3(0, 0, 0);
                material.diffuseColor = glm::vec3(0.1f, 0.1f, 0.1f);
                material.specularChance = glm::vec3(0.4f, 0, 0);
                material.smoothness = glm::vec3(1.0f, 0, 0);
                meshInfo.material = material;
                meshes.push_back(meshInfo);
                triangles.insert(triangles.end(), meshTriangles.begin(), meshTriangles.end());
            }
        

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

    }

    void RayScene::OnBufferSwap(Window& win) {

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

        computeShader.SetParameterInt(4, "NumberOfBounces");
        computeShader.SetParameterInt(3, "NumberOfRays");

        computeShader.Dispatch(SCREEN_WIDTH / 8, SCREEN_HEIGHT / 8, 1);

        tex.texUnit(shader, "tex0", 0);
        oldTex.texUnit(shader, "tex1", 1);
        averageTex.texUnit(shader, "tex2", 2);

        shader.SetParameterInt(Frame, "Frame");



        SceneVAO.Bind();
        glDrawElements(GL_TRIANGLES, 6, GL_UNSIGNED_INT, 0);

        computeShader.DeleteSSBOs();
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
