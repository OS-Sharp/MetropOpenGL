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
#include <memory>

// BVHTriangle Struct
struct alignas(16)  BVHTriangle {
    alignas(16) glm::vec3 A;
    alignas(16) glm::vec3 B;
    alignas(16) glm::vec3 C;

    BVHTriangle(const glm::vec3& a, const glm::vec3& b, const glm::vec3& c)
        : A(a), B(b), C(c) {}

    glm::vec3 Centre() const {
        return (A + B + C) / 3.0f;
    }
};

// Bounding Box Struct
struct BoundingBox {
    alignas(16) glm::vec3 Min = glm::vec3(std::numeric_limits<float>::infinity());
    alignas(16) glm::vec3 Max = glm::vec3(-std::numeric_limits<float>::infinity());

    glm::vec3 Centre() const {
        return (Min + Max) * 0.5f;
    }

    void GrowToInclude(const glm::vec3& point) {
        Min = glm::min(Min, point);
        Max = glm::max(Max, point);
    }

    void GrowToInclude(const BVHTriangle& triangle) {
        GrowToInclude(triangle.A);
        GrowToInclude(triangle.B);
        GrowToInclude(triangle.C);
    }
};

// BVH Node Struct
struct alignas(16) BVHNode {
    BoundingBox Bounds;
    int TriangleStartIndex = 0;
    int TriangleCount = 0;
    int ChildIndex = 0;  // -1 means leaf node

    bool isLeaf() const {
        return ChildIndex == 0;
    }
};

// BVH Node Struct
struct alignas(16) BVHModel {
    int NodeOffset;
    int TriangleOffset;
    Material material;
};

// BVH Class
class BVH {
public:
    std::vector<BVHTriangle> Triangles;
    std::vector<BVHModel> Models;
    std::vector<std::unique_ptr<BVHNode>> Nodes;
    std::vector<BVHNode> FlatNodes;

    std::vector<BVHNode> MoveToFlatNodes(std::vector<std::unique_ptr<BVHNode>>& Nodes) {
        std::vector<BVHNode> flatNodes;
        flatNodes.reserve(Nodes.size());

        for (auto& nodePtr : Nodes) {
            if (nodePtr) {
                flatNodes.push_back(std::move(*nodePtr));  // Move the BVHNode
                nodePtr.reset();  // Optional: free the memory explicitly
            }
        }

        return flatNodes;
    }

    BVH() {}

    BVH(std::vector<Triangle>& triangles, Material material) {
        AddModel(triangles, material);
    }

    BVHModel AddModel(std::vector<Triangle>& triangles, Material material) {
        std::unique_ptr<BVHNode> Root = std::make_unique<BVHNode>();

        int triOffset = Triangles.size();
        int nodeOffset = Nodes.size();

        Root->TriangleStartIndex = triOffset;

        // Create triangles
        for (size_t i = 0; i < triangles.size(); i++) {
            Triangles.emplace_back(triangles[i].P1,
                triangles[i].P2,
                triangles[i].P3);

            Root->Bounds.GrowToInclude(Triangles[i + triOffset]);
        }
        Root->TriangleCount = Triangles.size();

        // Create Root Node

        Nodes.push_back(std::move(Root));

        // Start splitting
        Split(Nodes[Nodes.size() - 1].get(), 0);

        BVHModel model;
        model.TriangleOffset = triOffset;
        model.NodeOffset = nodeOffset;
        model.material = material;

        FlatNodes = MoveToFlatNodes(Nodes);
        Models.push_back(model);

        return model;
    }
private:
    const int MaxDepth = 16;
    void Split(BVHNode* parent, int depth = 0) {
        if (depth >= MaxDepth || parent->TriangleCount <= 1)
            return;

        // --- Choose the split axis based on the longest extent ---
        // Compute the extents of the parent's bounding box
        glm::vec3 extents = parent->Bounds.Max - parent->Bounds.Min;

        // Determine the longest axis: 0 for X, 1 for Y, 2 for Z
        int splitAxis = 0;
        if (extents.y > extents.x && extents.y > extents.z) {
            splitAxis = 1;
        }
        else if (extents.z > extents.x && extents.z > extents.y) {
            splitAxis = 2;
        }

        // Use the centre along the longest axis as the splitting position
        float splitPos = parent->Bounds.Centre()[splitAxis];
        // -------------------------------------------------------------

        // Create child nodes
        auto childA = std::make_unique<BVHNode>();
        auto childB = std::make_unique<BVHNode>();

        // Record the current size BEFORE pushing child nodes
        int currentIndex = Nodes.size();

        // Now push the two children into the Nodes vector
        Nodes.push_back(std::move(childA)); // index currentIndex
        Nodes.push_back(std::move(childB)); // index currentIndex + 1

        // Assign ChildIndex to the first child's index
        parent->ChildIndex = currentIndex;

        BVHNode* leftChild = Nodes[parent->ChildIndex].get();
        BVHNode* rightChild = Nodes[parent->ChildIndex + 1].get();

        // Set initial triangle start index for the left child
        leftChild->TriangleStartIndex = parent->TriangleStartIndex;

        // Partitioning index: 'mid' marks the end of the left partition
        int mid = parent->TriangleStartIndex;
        for (int i = parent->TriangleStartIndex; i < parent->TriangleStartIndex + parent->TriangleCount; i++) {
            // Check the triangle centre along the chosen split axis
            bool isLeft = (Triangles[i].Centre()[splitAxis] < splitPos);
            if (isLeft) {
                // Grow the left child's bounds and swap triangles into the left partition
                leftChild->Bounds.GrowToInclude(Triangles[i]);
                std::swap(Triangles[i], Triangles[mid]);

                leftChild->TriangleCount++;
                mid++; // Advance partition boundary
            }
            else {
                // Grow the right child's bounds for triangles that go to the right partition
                rightChild->Bounds.GrowToInclude(Triangles[i]);
                rightChild->TriangleCount++;
            }
        }

        // Set the starting index for the right child
        rightChild->TriangleStartIndex = mid;

        // Recursively split the children
        Split(leftChild, depth + 1);
        Split(rightChild, depth + 1);
    }

};
