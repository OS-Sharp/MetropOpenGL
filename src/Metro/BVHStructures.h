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
#include <algorithm>

// Forward declaration for Material and Triangle (assumed defined elsewhere)
struct Material;
struct Triangle;

// BVHTriangle Struct
struct alignas(16) BVHTriangle {
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

    void GrowToInclude(const Triangle& triangle) {
        GrowToInclude(triangle.P1);
        GrowToInclude(triangle.P2);
        GrowToInclude(triangle.P3);
    }
};

// BVH Node Struct
struct alignas(16) BVHNode {
    BoundingBox Bounds;
    int TriangleStartIndex = 0;
    int TriangleCount = 0;
    int ChildIndex = 0;  // ChildIndex == 0 means leaf node

    bool isLeaf() const {
        return ChildIndex == 0;
    }
};

// BVH Model Struct
struct alignas(16) BVHModel {
    int NodeOffset;
    int TriangleOffset;
    Material material;
};

// BVH Class with SAH splitting and separate ChooseSplit function.
class BVH {
public:
    std::vector<Triangle> Triangles;
    std::vector<BVHModel> Models;
    std::vector<std::unique_ptr<BVHNode>> Nodes;
    std::vector<BVHNode> FlatNodes;

    std::vector<BVHNode> MoveToFlatNodes(std::vector<std::unique_ptr<BVHNode>>& Nodes) {
        std::vector<BVHNode> flatNodes;
        flatNodes.reserve(Nodes.size());

        for (auto& nodePtr : Nodes) {
            if (nodePtr) {
                flatNodes.push_back(std::move(*nodePtr));  // Move the BVHNode
                //nodePtr.reset();  // Optional: free the memory explicitly
            }
        }

        return flatNodes;
    }

    BVH() {}

    BVH(std::vector<Triangle>& triangles, Material material) {
        AddModel(triangles, material);
    }

    BVHModel AddModel(std::vector<Triangle>& triangles, Material material) {
        auto Root = std::make_unique<BVHNode>();

        int triOffset = Triangles.size();
        int nodeOffset = Nodes.size();

        Root->TriangleStartIndex = triOffset;

        // Create triangles and update the root bounds
        for (size_t i = 0; i < triangles.size(); i++) {
            Triangles.push_back(triangles[i]);

            Root->Bounds.GrowToInclude(triangles[i]);
        }
        Root->TriangleCount = static_cast<int>(triangles.size());

        // Create Root Node
        Nodes.push_back(std::move(Root));

        // Start splitting using SAH.
        Split(Nodes.back().get(), 0);

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

    // Helper: Compute the surface area of a bounding box.
    float SurfaceArea(const BoundingBox& box) const {
        glm::vec3 extents = box.Max - box.Min;
        return 2.0f * (extents.x * extents.y + extents.x * extents.z + extents.y * extents.z);
    }

    // Modified ChooseSplit that also returns the best cost.
    bool ChooseSplit(BVHNode* parent, int splitAxis, float parentSA, float& bestSplitPos, float& bestCost) {
        bestCost = std::numeric_limits<float>::infinity();
        bool foundCandidate = false;

        glm::vec3 extents = parent->Bounds.Max - parent->Bounds.Min;
        // Evaluate 5 candidate splits along the axis.
        for (int i = 1; i <= 5; i++) {
            float fraction = i / 6.0f;
            float candidatePos = parent->Bounds.Min[splitAxis] + fraction * extents[splitAxis];

            BoundingBox leftBox;
            BoundingBox rightBox;
            int leftCount = 0;
            int rightCount = 0;

            int start = parent->TriangleStartIndex;
            int end = start + parent->TriangleCount;
            for (int j = start; j < end; j++) {
                float center = Triangles[j].Centre()[splitAxis];
                if (center < candidatePos) {
                    leftBox.GrowToInclude(Triangles[j]);
                    leftCount++;
                }
                else {
                    rightBox.GrowToInclude(Triangles[j]);
                    rightCount++;
                }
            }

            // Skip candidate splits that would leave an empty child.
            if (leftCount == 0 || rightCount == 0)
                continue;

            float cost = leftCount * SurfaceArea(leftBox) + rightCount * SurfaceArea(rightBox);
            if (cost < bestCost) {
                bestCost = cost;
                bestSplitPos = candidatePos;
                foundCandidate = true;
            }
        }

        // Reject the split if no candidate was found or if the cost isn't lower than not splitting.
        if (!foundCandidate || bestCost >= parent->TriangleCount * parentSA)
            return false;

        return true;
    }

    // Modified Split function: tests all three axes.
    void Split(BVHNode* parent, int depth = 0) {
        if (depth >= MaxDepth || parent->TriangleCount <= 2)
            return;

        float parentSA = SurfaceArea(parent->Bounds);
        float bestCostOverall = std::numeric_limits<float>::infinity();
        int bestAxis = -1;
        float bestSplitPosForAxis = 0.0f;

        // Try splitting along each axis and choose the best candidate.
        for (int axis = 0; axis < 3; axis++) {
            float candidateSplitPos = 0.0f;
            float candidateCost = std::numeric_limits<float>::infinity();
            if (ChooseSplit(parent, axis, parentSA, candidateSplitPos, candidateCost)) {
                if (candidateCost < bestCostOverall) {
                    bestCostOverall = candidateCost;
                    bestAxis = axis;
                    bestSplitPosForAxis = candidateSplitPos;
                }
            }
        }

        // If no beneficial split was found, stop.
        if (bestAxis == -1)
            return;

        // Create child nodes.
        auto childA = std::make_unique<BVHNode>();
        auto childB = std::make_unique<BVHNode>();

        int currentIndex = static_cast<int>(Nodes.size());
        Nodes.push_back(std::move(childA)); // Left child at index currentIndex.
        Nodes.push_back(std::move(childB)); // Right child at index currentIndex + 1.
        parent->ChildIndex = currentIndex;

        BVHNode* leftChild = Nodes[parent->ChildIndex].get();
        BVHNode* rightChild = Nodes[parent->ChildIndex + 1].get();

        // Partition triangles based on the best axis and split position.
        int start = parent->TriangleStartIndex;
        int end = start + parent->TriangleCount;
        int mid = start;
        for (int i = start; i < end; i++) {
            if (Triangles[i].Centre()[bestAxis] < bestSplitPosForAxis) {
                std::swap(Triangles[i], Triangles[mid]);
                mid++;
            }
        }

        int leftCount = mid - start;
        int rightCount = parent->TriangleCount - leftCount;
        // If partitioning fails, do not split further.
        if (leftCount == 0 || rightCount == 0)
            return;

        // Setup child nodes with their triangle ranges.
        leftChild->TriangleStartIndex = start;
        leftChild->TriangleCount = leftCount;
        rightChild->TriangleStartIndex = mid;
        rightChild->TriangleCount = rightCount;

        // Recompute the bounds for each child.
        for (int i = start; i < mid; i++) {
            leftChild->Bounds.GrowToInclude(Triangles[i]);
        }
        for (int i = mid; i < end; i++) {
            rightChild->Bounds.GrowToInclude(Triangles[i]);
        }

        // Recursively split the child nodes.
        Split(leftChild, depth + 1);
        Split(rightChild, depth + 1);
    }
};
