#version 430 core

struct Material{
    vec3 emmisionColor;
    vec3 emmisionStrength;
    vec3 diffuseColor;
    vec3 smoothness;
    vec3 specularProbability;
    vec3 specularColor;
    vec3 opacity;
};

struct Sphere{
    Material material;
    vec3 position;
    vec3 radius;
};

struct DebugBox {
    Material material;
    vec3 position;
    vec3 size;
};

struct Triangle{
    vec3 posA, posB, posC;
    vec3 normA, normB, normC;
};

struct MeshInfo{
    vec3 boundsMin;
    vec3 boundsMax;
    Material material;
    uint firstTriangleIndex;
    uint numTriangles;
    vec2 padding;
};

struct Camera{
    vec3 position;
    vec3 direction;
    vec3 fov;
};

struct Ray{
    vec3 origin;
    vec3 direction;
};

struct HitInfo{
    vec3 hitPoint;
    vec3 normal;
    bool didHit;
    float dst;
    Material material;
};

layout(local_size_x = 8, local_size_y = 8, local_size_z = 1) in;
layout(rgba32f, binding = 0) uniform image2D screen;
layout(rgba32f, binding = 1) uniform image2D oldScreen;
layout(rgba32f, binding = 2) uniform image2D averageScreen;

layout(std430, binding = 3) buffer CameraData {
    Camera camera;
};
layout(std430, binding = 4) buffer CircleData {
    Sphere spheres[];
};
layout(std430, binding = 5) buffer CircleLength {
    uint NumSpheres;
};

layout(std430, binding = 6) buffer Frames {
    uint Frame;
};
layout(std430, binding = 7) buffer MeshData {
    MeshInfo Meshes[];
};
layout(std430, binding = 8) buffer TriangleData {
    Triangle Triangles[];
};
layout(std430, binding = 9) buffer MeshLength {
    uint NumMeshes;
};
layout(std430, binding = 10) buffer BoxData {
    DebugBox DebugBoxes[];
};
layout(std430, binding = 11) buffer BoxLength {
    uint NumBoxes;
};

uniform vec3 SkyColourHorizon;
uniform vec3 SkyColourZenith;
uniform vec3 SunLightDirection;
uniform vec3 GroundColor;

uniform float SunFocus;
uniform float SunIntensity;
uniform float SunThreshold;

uniform mat4 viewProj;

uniform int NumberOfBounces = 4;
uniform int NumberOfRays = 5;
uniform int DebugMode = 0;

//=========================      =========================
//========================= HELP =========================
//=========================      =========================
highp float rand(inout vec2 co)
{
    co = fract(co * vec2(123.34, 456.21)); // Mix seed
    co += dot(co, co.yx + vec2(21.1, 71.3)); // Add variation
    co = fract(co * vec2(143.543, 927.442)); // Another transformation
    return fract(sin(dot(co, vec2(127.1, 311.7))) * 43758.5453);
}

float randNorm(inout vec2 co){
    float theta = 3.14 * 2 * rand(co);
    float rho = sqrt(-2 * log(rand(co)));
    return rho * cos(theta);
}

vec3 RandomDirection(inout vec2 state) {
    return normalize(vec3(randNorm(state), randNorm(state), randNorm(state)));
}

vec3 GetAmbientLight(Ray ray) {
    float gradient = pow(smoothstep(0.0f, 0.4f, float(ray.direction.y)), 0.35f);
    vec3 gradientC = mix(SkyColourHorizon, SkyColourZenith, gradient);
    float sun = pow(max(0, dot(ray.direction, -SunLightDirection) - SunThreshold), SunFocus) * SunIntensity;

    float groundToSkyT = smoothstep(-0.01f, 0.0f, float(ray.direction.y));
    float sunMask = groundToSkyT >= 1 ? 1 : 0;
    return mix(GroundColor, gradientC, groundToSkyT) + sun * sunMask;
}

vec3 RandomDirectionHemsiphere(vec3 normal, inout vec2 state) {
    vec3 dir = RandomDirection(state);
    return dir * sign(dot(dir, normal));
}

bool RayIntersectsAABB(vec3 rayOrigin, vec3 rayDir, vec3 minBounds, vec3 maxBounds, out float tmin, out float tmax)
{
    // Initialize tmin and tmax for the ray
    tmin = 0.0;
    tmax = 1000000.0; // A large value representing the farthest possible distance

    // Iterate over the three axes (X, Y, Z)
    for (int i = 0; i < 3; i++)
    {
        if (abs(rayDir[i]) > 1e-6) // Avoid division by zero (ray parallel to an axis)
        {
            float t1 = (minBounds[i] - rayOrigin[i]) / rayDir[i];
            float t2 = (maxBounds[i] - rayOrigin[i]) / rayDir[i];

            // Swap t1 and t2 so t1 is the near intersection and t2 is the far intersection
            if (t1 > t2) {
                float tmp = t1;
                t1 = t2;
                t2 = tmp;
            }

            // Update tmin and tmax
            tmin = max(tmin, t1); // The largest "near" intersection
            tmax = min(tmax, t2); // The smallest "far" intersection

            // If tmin is greater than tmax, the ray misses the box
            if (tmin > tmax)
                return false;
        }
        else
        {
            // Ray is parallel to the slab, check if the origin is outside the slab
            if (rayOrigin[i] < minBounds[i] || rayOrigin[i] > maxBounds[i])
                return false;
        }
    }

    // If we get here, there was an intersection
    return true;
}

//=========================      =========================
//========================= HELP ========================= END
//=========================      =========================

//=========================                                =========================
//========================= RAY OBJECT INTERSECT FUNCTIONS =========================
//=========================                                =========================


HitInfo RayBox(Ray ray, vec3 minBounds, vec3 maxBounds, Material material)
{
    HitInfo hitInfo;
    hitInfo.didHit = false;

    float tmin = -1000000.0;  // Set to large negative number to allow rays inside
    float tmax = 1000000.0;

    for (int i = 0; i < 3; i++) // Loop over x, y, z axes
    {
        if (abs(ray.direction[i]) > 1e-6) // Avoid division by zero
        {
            float t1 = (minBounds[i] - ray.origin[i]) / ray.direction[i];
            float t2 = (maxBounds[i] - ray.origin[i]) / ray.direction[i];

            if (t1 > t2) { float tmp = t1; t1 = t2; t2 = tmp; }

            tmin = max(tmin, t1);
            tmax = min(tmax, t2);

            if (tmin > tmax) return hitInfo; // No intersection
        }
        else if (ray.origin[i] < minBounds[i] || ray.origin[i] > maxBounds[i])
        {
            return hitInfo; // Ray is outside and parallel to the box
        }
    }

    // Ensure correct exit for inside-box cases
    if (tmin < 0.0) tmin = tmax;
    if (tmin < 0.0) return hitInfo;

    // Compute hit point
    hitInfo.didHit = true;
    hitInfo.dst = tmin;
    hitInfo.hitPoint = ray.origin + ray.direction * tmin;
    hitInfo.material = material;

    // Compute normal at intersection (find the closest box face)
    vec3 center = (minBounds + maxBounds) * 0.5;
    vec3 relativeHit = hitInfo.hitPoint - center;

    vec3 faceNormals[6] = vec3[6](
        vec3(1, 0, 0), vec3(-1, 0, 0),
        vec3(0, 1, 0), vec3(0, -1, 0),
        vec3(0, 0, 1), vec3(0, 0, -1)
        );

    float maxComponent = -1.0;
    for (int i = 0; i < 6; i++)
    {
        float d = abs(dot(relativeHit, faceNormals[i]));
        if (d > maxComponent)
        {
            maxComponent = d;
            hitInfo.normal = faceNormals[i];
        }
    }

    if (dot(hitInfo.normal, ray.direction) > 0.0)
    {
        hitInfo.normal = -hitInfo.normal;
    }

    return hitInfo;
}


HitInfo RaySphere(Ray ray, vec3 sphereCenter, float sphereRadius, Material material){
    HitInfo hitInfo;
    hitInfo.didHit = false;

    vec3 o_c = ray.origin - sphereCenter;
    float b = dot(ray.direction, o_c);
    float c = dot(o_c, o_c) - sphereRadius * sphereRadius;
    float intersectionState = b * b - c;
    
    if (intersectionState >= 0.0)
    {
        float t1 = (-b - sqrt(intersectionState)); // Near intersection
        float t2 = (-b + sqrt(intersectionState)); // Far intersection

        // Ensure we get the first positive hit
        float dst = (t1 >= 0) ? t1 : t2;

        if (dst >= 0)
        {
            hitInfo.didHit = true;
            hitInfo.dst = dst;
            hitInfo.hitPoint = ray.origin + ray.direction * dst;
            hitInfo.normal = normalize(hitInfo.hitPoint - sphereCenter);
            hitInfo.material = material;
        }
    }


    return hitInfo;
}

HitInfo RayTriangle(Ray ray, Triangle tri, Material material) {
    HitInfo hitInfo;
    hitInfo.didHit = false; // Default to no hit

    // Edges of the triangle
    vec3 edgeAB = tri.posB - tri.posA;
    vec3 edgeAC = tri.posC - tri.posA;
    
    // Calculate determinant, pvec, and tvec
    vec3 pvec = cross(ray.direction, edgeAC);
    float det = dot(edgeAB, pvec);

    // Avoid precision issues with a slightly larger epsilon
    if (abs(det) < 1e-4) {
        return hitInfo; // No hit
    }

    float invDet = 1.0 / det;

    // Calculate tvec and u
    vec3 tvec = ray.origin - tri.posA;
    float u = dot(tvec, pvec) * invDet;

    // If u is outside the triangle, return no hit
    if (u < 0.0 || u > 1.0) {
        return hitInfo;
    }

    vec3 qvec = cross(tvec, edgeAB);
    float v = dot(ray.direction, qvec) * invDet;

    if (v < 0.0 || u + v > 1.0) {
        return hitInfo;
    }

    float t = dot(edgeAC, qvec) * invDet;

    if (t < 1e-4) {
        return hitInfo; // Ignore intersections too close to the camera
    }

    hitInfo.didHit = true;
    hitInfo.dst = t;
    hitInfo.hitPoint = ray.origin + ray.direction * t;

    float w = 1.0 - u - v;
    hitInfo.normal = normalize(tri.normA * w + tri.normB * u + tri.normC * v);

    if (dot(hitInfo.normal, ray.direction) > 0.0) {
        hitInfo.normal = -hitInfo.normal;
    }

    hitInfo.material = material;

    return hitInfo;
}

//=========================                                =========================
//========================= RAY OBJECT INTERSECT FUNCTIONS ========================= END
//=========================                                =========================


//=========================                      =========================
//========================= RAY OBJECT INTERSECT =========================
//=========================                      =========================
bool DebugRay(vec3 rayOrigin, vec3 rayDir, vec3 start, vec3 end, out float debugT)
{
    vec3 ab = end - start;
    vec3 ao = rayOrigin - start;
    vec3 ab_cross_d = cross(ab, rayDir);
    vec3 ao_cross_d = cross(ao, rayDir);

    float ab_dot_ab = dot(ab, ab);
    float ab_dot_ao = dot(ab, ao);
    float ab_dot_d = dot(ab, rayDir);

    float t = (ab_dot_ao * ab_dot_d - ab_dot_ab * dot(ao, rayDir)) /
        (ab_dot_ab * dot(rayDir, rayDir) - ab_dot_d * ab_dot_d);

    vec3 closestPoint = rayOrigin + rayDir * t;
    vec3 projOnSegment = start + ab * clamp(dot(closestPoint - start, ab) / ab_dot_ab, 0.0, 1.0);

    float dist = length(closestPoint - projOnSegment);

    debugT = t;
    return dist < 0.01; // Line thickness threshold
}

HitInfo RayAllBoxes(Ray ray)
{
    HitInfo closestHit;
    closestHit.didHit = false;
    closestHit.dst = 1.0 / 0.0; // Set to infinity
    closestHit.hitPoint = vec3(0.0);
    closestHit.normal = vec3(0.0);

    for (int i = 0; i < NumBoxes; i++)
    {
        DebugBox box = DebugBoxes[i];
        vec3 minBounds = box.position - box.size * 0.5;
        vec3 maxBounds = box.position + box.size * 0.5;

        HitInfo hitInfo = RayBox(ray, minBounds, maxBounds, box.material);

        if (hitInfo.didHit && hitInfo.dst < closestHit.dst)
        {
            closestHit = hitInfo;
        }
    }

    return closestHit;
}


HitInfo RayAllSpheres(Ray ray){
    HitInfo closestHit;
    closestHit.didHit = false;
    closestHit.hitPoint = vec3(0.0);
    closestHit.normal = vec3(0.0);
    closestHit.dst = 1.0 / 0.0;

    for(int i = 0; i < NumSpheres; i++){
        Sphere sphere = spheres[i];
        HitInfo hitInfo = RaySphere(ray, sphere.position, sphere.radius.x, sphere.material);

        if(hitInfo.didHit && hitInfo.dst < closestHit.dst){
            closestHit = hitInfo;
        }
    }
    return closestHit;
}

HitInfo RayAllMeshes(Ray ray, inout int numTriTests){
    HitInfo closestHit;
    closestHit.didHit = false;
    closestHit.hitPoint = vec3(0.0);
    closestHit.normal = vec3(0.0);
    closestHit.dst = 1.0 / 0.0;

    for(int mesh = 0; mesh < NumMeshes; mesh++){
        MeshInfo meshInfo = Meshes[mesh];
        float tmin, tmax;
        if(!RayIntersectsAABB(ray.origin, ray.direction, meshInfo.boundsMin, meshInfo.boundsMax, tmin, tmax)) {
            continue;
        }

        for(int i = 0; i < meshInfo.numTriangles; i++){
            numTriTests++;
            uint tri = meshInfo.firstTriangleIndex + i;
            Triangle triangle = Triangles[tri];
            HitInfo hitInfo = RayTriangle(ray,triangle, meshInfo.material);

            if(hitInfo.didHit && hitInfo.dst < closestHit.dst)
            {
                closestHit = hitInfo;
            }
        }

    }
    return closestHit;
}

//=========================                      =========================
//========================= RAY OBJECT INTERSECT =========================
//=========================                      =========================



vec3 DebugTrace(Ray ray, inout vec2 state)
{
    vec3 rayColor = vec3(1, 0, 0); // Red for debugging
    vec3 rayLight = vec3(0, 0, 0);

    for (int i = 0; i < NumberOfBounces; i++)
    {
        HitInfo hitInfo = RayAllSpheres(ray);
        HitInfo hitInfoMesh = RayAllMeshes(ray, state.x);

        if (hitInfo.didHit || hitInfoMesh.didHit)
        {
            vec3 hitPos = hitInfo.didHit ? hitInfo.hitPoint : hitInfoMesh.hitPoint;
            vec3 normal = hitInfo.didHit ? hitInfo.normal : hitInfoMesh.normal;

            // Store the hit location as a visible dot
            if (length(hitPos - camera.position) < 5.0) // Limit debug range
                return vec3(1, 1, 0); // Yellow dot to indicate a hit

            // Continue ray bounce
            ray.origin = hitPos;
            ray.direction = reflect(ray.direction, normal);
        }
        else
        {
            // If no hit, return sky color for debugging
            return vec3(0.2, 0.4, 0.8);
        }
    }
    return vec3(0, 0, 0);
}
// Add Schlick's approximation function
float SchlickApproximation(float cosine, float refractiveIndex) {
    float r0 = (1.0 - refractiveIndex) / (1.0 + refractiveIndex);
    r0 = r0 * r0;
    return r0 + (1.0 - r0) * pow(1.0 - cosine, 5.0);
}

vec3 FullTrace(Ray ray, inout vec2 state) {
    vec3 rayColor = vec3(1.0);
    vec3 rayLight = vec3(0.0);
    int numTriTests = 0;
    bool rayInsideObject = false; // Track if the ray is inside a refractive material


    // Example debug line from point A to B
    vec3 debugStart = vec3(-1.0, 1.0, -3.0);
    vec3 debugEnd = vec3(1.0, 1.0, -3.0);

    float debugT;
    if (DebugRay(ray.origin, ray.direction, debugStart, debugEnd, debugT)) {
        return vec3(1.0, 0.0, 0.0); // Render debug lines as red
    }

    for (int i = 0; i < NumberOfBounces; i++) {
        HitInfo hitInfo;
        hitInfo.didHit = false;
        hitInfo.dst = 1e20;
        HitInfo hitInfoSphere = RayAllSpheres(ray);
        HitInfo hitInfoMesh = RayAllMeshes(ray, numTriTests);
        HitInfo hitInfoBox = RayAllBoxes(ray);

        if (hitInfoSphere.didHit) hitInfo = hitInfoSphere;
        if (hitInfoMesh.didHit && hitInfoMesh.dst < hitInfo.dst) hitInfo = hitInfoMesh;
        if (hitInfoBox.didHit && hitInfoBox.dst < hitInfo.dst) hitInfo = hitInfoBox;

        if (hitInfo.didHit) {
            vec3 emission = hitInfo.material.emmisionColor * hitInfo.material.emmisionStrength.x;
            rayLight += emission * rayColor;

            vec3 normal = hitInfo.normal;
            float opacity = hitInfo.material.opacity.x;

            // Check if material is transparent
            if (opacity < 1.0) {
                float refractiveIndex = 1.5; // Glass refractive index
                bool isEntering = dot(ray.direction, normal) < 0.0;
                vec3 surfaceNormal = isEntering ? normal : -normal;
                float eta = isEntering ? (1.0 / refractiveIndex) : refractiveIndex;

                vec3 refractedDir = refract(ray.direction, surfaceNormal, eta);
                float cosTheta = abs(dot(normalize(-ray.direction), surfaceNormal));
                float fresnel = SchlickApproximation(cosTheta, eta);

                // Russian Roulette for reflection/refraction
                if (rand(state) < fresnel) {
                    vec3 diffuseDir = normalize(normal + RandomDirection(state));
                    vec3 specularDir = reflect(ray.direction, normal);
                    bool isSpecular = hitInfo.material.specularProbability.x >= rand(state);
                    ray.direction = mix(diffuseDir, specularDir, hitInfo.material.smoothness.x * float(isSpecular));
                }
                else {
                    ray.direction = normalize(ray.direction);
                    rayInsideObject = !rayInsideObject; // Toggle ray inside state
                }

                // Adjust ray origin to prevent self-intersection
                ray.origin = hitInfo.hitPoint + ray.direction * 1e-4;
                rayColor *= hitInfo.material.diffuseColor; // Apply color tint for transparency
            }
            else {
                // Handle opaque materials (original code)
                vec3 diffuseDir = normalize(normal + RandomDirection(state));
                vec3 specularDir = reflect(ray.direction, normal);
                bool isSpecular = hitInfo.material.specularProbability.x >= rand(state);
                ray.direction = mix(diffuseDir, specularDir, hitInfo.material.smoothness.x * float(isSpecular));
                rayColor *= mix(hitInfo.material.diffuseColor, hitInfo.material.specularColor, float(isSpecular)) * opacity;
            }
        }
        else {
            // No hit, accumulate sky color
            rayLight += rayColor * GetAmbientLight(ray);
            break;
        }
    }
    return rayLight;
}


void main() {
    ivec2 pixel_coords = ivec2(gl_GlobalInvocationID.xy);
    ivec2 dims = imageSize(screen);

    // Convert pixel coordinates to normalized device coordinates (-1.0, 1.0)
    float u = (float(pixel_coords.x) / float(dims.x)) * 2.0 - 1.0;
    float v = (float(pixel_coords.y) / float(dims.y)) * 2.0 - 1.0;

    // Aspect ratio correction
    float aspectRatio = float(dims.x) / float(dims.y);
    u *= aspectRatio;

    // Calculate the direction of the ray from the camera through the screen pixel
    float fovTan = tan(radians(camera.fov.x) * 0.5);
    vec3 forward = normalize(camera.direction);
    vec3 right = normalize(cross(forward, vec3(0,1,0)));
    vec3 up = cross(right, forward);

    vec3 rayDir = normalize(forward + u * fovTan * right + v * fovTan * up);


    // Perform path tracing
    vec2 state = vec2(u + Frame * 0.09201489f, v + Frame * 0.06101789f);
    vec3 color = vec3(0,0,0);
    for(int i = 0; i < NumberOfRays; i++){
     Ray ray;
     ray.origin = camera.position;
     ray.direction = rayDir;

     //color += DebugTrace(ray, state);
     color += FullTrace(ray, state);

    }
    color /= float(NumberOfRays); 

    // Output to image
    vec3 pixel = clamp(color, 0, 1);

    vec3 old = imageLoad(oldScreen, pixel_coords).rgb;

    float weight = 1.0 / (Frame + 1.0f);
    vec3 average = old * (1.0f - weight) + pixel * weight;

    imageStore(screen, pixel_coords, vec4(average,1));
    //imageStore(oldScreen, pixel_coords, pixel);
}