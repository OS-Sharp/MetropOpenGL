#pragma once
#include<glad/glad.h>
#include<GLFW/glfw3.h>
#include<glm/glm.hpp>
#include<glm/gtc/matrix_transform.hpp>
#include<glm/gtc/type_ptr.hpp>
#include<glm/gtx/rotate_vector.hpp>
#include<glm/gtx/vector_angle.hpp>
#include <vector>

struct alignas(16) Material {
	alignas(16) glm::vec3 emmisionStrength;
	alignas(16) glm::vec3 emmisionColor;
	alignas(16) glm::vec3 diffuseColor;
	alignas(16) glm::vec3 smoothness;
	alignas(16) glm::vec3 specularChance = glm::vec3(0,0,0);
	alignas(16) glm::vec3 specularColor = glm::vec3(1, 1, 1);;
	alignas(16) glm::vec3 opacity = glm::vec3(1, 1, 1);;
};

struct alignas(16) TraceCircle {
	alignas(16) Material material;
	alignas(16) glm::vec3 position;
	alignas(16) float radius;
};

struct alignas(16) TraceDebugBox {
	alignas(16) Material material;
	alignas(16) glm::vec3 position;
	alignas(16) glm::vec3 size;
};

struct alignas(16) CameraSettings {
	alignas(16) glm::vec3 position;
	alignas(16) glm::vec3 direction;
	alignas(16) float fov;
};

struct alignas(16) Triangle {
	alignas(16) glm::vec3 P1;
	alignas(16) glm::vec3 P2;
	alignas(16) glm::vec3 P3;

	alignas(16) glm::vec3 NormP1;
	alignas(16) glm::vec3 NormP2;
	alignas(16) glm::vec3 NormP3;

	// Static function to compute the normal of the triangle
	static glm::vec3 getNormal(const Triangle& tri) {
		glm::vec3 edge1 = tri.P2 - tri.P1;
		glm::vec3 edge2 = tri.P3 - tri.P1;
		glm::vec3 normal = glm::normalize(glm::cross(edge1, edge2));
		return normal;
	}
};

struct alignas(16) MeshInfo {
	alignas(16) glm::vec3 bMin;
	alignas(16) glm::vec3 bMax;
	alignas(16) Material material;
	unsigned int startIndex;
	unsigned int trisNumber;

	static MeshInfo createMeshFromTris(unsigned int bufferOffset, std::vector<Triangle> tris) {
		MeshInfo mesh;
		mesh.trisNumber = tris.size();
		std::vector<glm::vec3> points = {};
		for (int i = 0; i < tris.size(); i++) {
			points.push_back(tris[i].P1);
			points.push_back(tris[i].P2);
			points.push_back(tris[i].P3);
		}

		mesh.bMin = getMinBound(points);
		mesh.bMax = getMaxBound(points);
		mesh.startIndex = bufferOffset;

		return mesh;
	}

	static glm::vec3 getMinBound(std::vector<glm::vec3> vs) {
		float minX = std::numeric_limits<float>::max();
		float minY = std::numeric_limits<float>::max();
		float minZ = std::numeric_limits<float>::max();

		for (int i = 0; i < vs.size(); i++) {
			if (vs[i].x < minX) {
				minX = vs[i].x;
			}
			if (vs[i].y < minY) {
				minY = vs[i].x;
			}
			if (vs[i].z < minZ) {
				minZ = vs[i].z;
			}
		}

		return glm::vec3(minX, minY, minZ);
	}

	static glm::vec3 getMaxBound(std::vector<glm::vec3> vs) {
		float maxX = std::numeric_limits<float>::min();
		float maxY = std::numeric_limits<float>::min();
		float maxZ = std::numeric_limits<float>::min();

		for (int i = 0; i < vs.size(); i++) {
			if (vs[i].x > maxX) {
				maxX = vs[i].x;
			}
			if (vs[i].y > maxY) {
				maxY = vs[i].x;
			}
			if (vs[i].z > maxZ) {
				maxZ = vs[i].z;
			}
		}

		return glm::vec3(maxX, maxY, maxZ);
	}
};