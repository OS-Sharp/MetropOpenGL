#pragma once
#include <glad/glad.h>
#include <GLFW/glfw3.h>
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>
#include <glm/gtx/rotate_vector.hpp>
#include <glm/gtx/vector_angle.hpp>
#include <vector>
#include <limits>

//-----------------------------------------------------------------------------
// Material
//-----------------------------------------------------------------------------
// Represents the properties of a surface material used in shading calculations.
// Each member is aligned to 16 bytes for compatibility with GPU data layouts.
struct alignas(16) Material {
    // Intensity of the light the material emits.
    alignas(16) glm::vec3 emmisionStrength;
    // Color of the light the material emits.
    alignas(16) glm::vec3 emmisionColor;
    // Base color of the material (diffuse reflectance).
    alignas(16) glm::vec3 diffuseColor;
    // Smoothness factor, influencing specular reflection.
    alignas(16) glm::vec3 smoothness;
    // The probability that the material reflects specularly.
    alignas(16) glm::vec3 specularChance = glm::vec3(0, 0, 0);
    // Color of specular reflection.
    alignas(16) glm::vec3 specularColor = glm::vec3(1, 1, 1);
    // Opacity of the material (1.0 means fully opaque).
    alignas(16) glm::vec3 opacity = glm::vec3(1, 1, 1);
    // Texture slot index into a texture array. -1 indicates no texture.
    GLuint textureSlot = -1;
};

//-----------------------------------------------------------------------------
// TraceCircle
//-----------------------------------------------------------------------------
// Represents a circular geometry (e.g., a sphere projection or disk) for tracing.
// Used for sampling lights or debugging purposes.
struct alignas(16) TraceCircle {
    // Material properties associated with this circle.
    alignas(16) Material material;
    // World-space position of the circle's center.
    alignas(16) glm::vec3 position;
    // Radius of the circle.
    alignas(16) float radius;
};

//-----------------------------------------------------------------------------
// TraceDebugBox
//-----------------------------------------------------------------------------
// Represents a box geometry used for debugging (e.g., visualizing bounding boxes).
struct alignas(16) TraceDebugBox {
    // Material properties for the debug box.
    alignas(16) Material material;
    // World-space position of the box's center.
    alignas(16) glm::vec3 position;
    // Dimensions of the box (width, height, depth).
    alignas(16) glm::vec3 size;
};

//-----------------------------------------------------------------------------
// CameraSettings
//-----------------------------------------------------------------------------
// Stores the basic parameters for a camera used in rendering.
struct alignas(16) CameraSettings {
    // Camera position in world space.
    alignas(16) glm::vec3 position;
    // Viewing direction of the camera.
    alignas(16) glm::vec3 direction;
    // Field-of-view (in degrees).
    alignas(16) float fov;
};

//-----------------------------------------------------------------------------
// Triangle
//-----------------------------------------------------------------------------
// Represents a triangle primitive for ray intersections.
// Contains vertex positions, normals at each vertex, and texture coordinates.
struct alignas(16) Triangle {
    // Positions of the triangle vertices.
    alignas(16) glm::vec3 P1;
    alignas(16) glm::vec3 P2;
    alignas(16) glm::vec3 P3;

    // Normals at each vertex (can be used for smooth shading).
    alignas(16) glm::vec3 NormP1;
    alignas(16) glm::vec3 NormP2;
    alignas(16) glm::vec3 NormP3;

    // Texture coordinates for each vertex.
    // Note: A triangle typically has three UV coordinates.
    // UVP4 is provided here for padding
    alignas(8) glm::vec2 UVP1;
    alignas(8) glm::vec2 UVP2;
    alignas(8) glm::vec2 UVP3;
    alignas(8) glm::vec2 UVP4;

    // Computes and returns the face normal using the cross product of two edges.
    static glm::vec3 getNormal(const Triangle& tri) {
        glm::vec3 edge1 = tri.P2 - tri.P1;
        glm::vec3 edge2 = tri.P3 - tri.P1;
        glm::vec3 normal = glm::normalize(glm::cross(edge1, edge2));
        return normal;
    }

    // Returns the centroid of the triangle.
    glm::vec3 Centre() const {
        return (P1 + P2 + P3) / 3.0f;
    }
};

//-----------------------------------------------------------------------------
// MeshInfo
//-----------------------------------------------------------------------------
// Contains information about a mesh, including its bounding box,
// associated material, the starting index in a triangle buffer, and the number of triangles.
struct alignas(16) MeshInfo {
    // Minimum corner of the mesh's axis-aligned bounding box.
    alignas(16) glm::vec3 bMin;
    // Maximum corner of the mesh's axis-aligned bounding box.
    alignas(16) glm::vec3 bMax;
    // Material associated with the mesh.
    alignas(16) Material material;
    // Starting index in a global triangle buffer.
    unsigned int startIndex;
    // Number of triangles in the mesh.
    unsigned int trisNumber;

    // Static function to create a MeshInfo instance from a vector of triangles.
    static MeshInfo createMeshFromTris(unsigned int bufferOffset, std::vector<Triangle> tris) {
        MeshInfo mesh;
        mesh.trisNumber = tris.size();
        std::vector<glm::vec3> points = {};
        // Gather all vertex positions from the triangles.
        for (int i = 0; i < tris.size(); i++) {
            points.push_back(tris[i].P1);
            points.push_back(tris[i].P2);
            points.push_back(tris[i].P3);
        }

        // Compute the axis-aligned bounding box for the mesh.
        mesh.bMin = getMinBound(points);
        mesh.bMax = getMaxBound(points);
        mesh.startIndex = bufferOffset;

        return mesh;
    }

    // Computes the minimum bounds (smallest x, y, z) from a set of points.
    static glm::vec3 getMinBound(std::vector<glm::vec3> vs) {
        float minX = std::numeric_limits<float>::max();
        float minY = std::numeric_limits<float>::max();
        float minZ = std::numeric_limits<float>::max();

        for (int i = 0; i < vs.size(); i++) {
            if (vs[i].x < minX) {
                minX = vs[i].x;
            }
            // NOTE: There seems to be an error here: should compare vs[i].y for minY.
            if (vs[i].y < minY) {
                minY = vs[i].y;
            }
            if (vs[i].z < minZ) {
                minZ = vs[i].z;
            }
        }

        return glm::vec3(minX, minY, minZ);
    }

    // Computes the maximum bounds (largest x, y, z) from a set of points.
    static glm::vec3 getMaxBound(std::vector<glm::vec3> vs) {
        float maxX = std::numeric_limits<float>::lowest();
        float maxY = std::numeric_limits<float>::lowest();
        float maxZ = std::numeric_limits<float>::lowest();

        for (int i = 0; i < vs.size(); i++) {
            if (vs[i].x > maxX) {
                maxX = vs[i].x;
            }
            // NOTE: There seems to be an error here: should compare vs[i].y for maxY.
            if (vs[i].y > maxY) {
                maxY = vs[i].y;
            }
            if (vs[i].z > maxZ) {
                maxZ = vs[i].z;
            }
        }

        return glm::vec3(maxX, maxY, maxZ);
    }
};
