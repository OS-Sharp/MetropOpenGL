#version 430 core

struct Material{
    vec3 emmisionColor;
    vec3 emmisionStrength;
    vec3 diffuseColor;
    vec3 smoothness;
    vec3 specularProbability;
    vec3 specularColor;
};

struct Sphere{
    Material material;
    vec3 position;
    vec3 radius;
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

highp float rand(inout vec2 co)
{
    highp float a = 12.9898;
    highp float b = 78.233;
    highp float c = 43758.5453;
    highp float dt1 = dot(co.xy ,vec2(a,b));
    highp float dt2 = dot(co.xy + vec2(0.11324234,-0.21312664),vec2(a,b));
    highp float sn1= mod(dt1,3.14);
    highp float sn2= mod(dt2,3.14);

    co = vec2(fract(sin(sn1) * c), -fract(sin(sn2) * c));
    return co.x;
}

float randNorm(inout vec2 co){
    float theta = 3.14 * 2 * rand(co);
    float rho = sqrt(-2 * log(rand(co)));
    return rho * cos(theta);
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

HitInfo RaySphere(Ray ray, vec3 sphereCenter, float sphereRadius, Material material){
    HitInfo hitInfo;
    hitInfo.didHit = false;

    vec3 o_c = ray.origin - sphereCenter;
    float b = dot(ray.direction, o_c);
    float c = dot(o_c, o_c) - sphereRadius * sphereRadius;
    float intersectionState = b * b - c;
    
    if (intersectionState >= 0.0)
    {
        float dst = (-b - sqrt(intersectionState));
        vec3 intersection = ray.origin + ray.direction * dst; // fix the intersection calculation
        if(dst >= 0){
            hitInfo.didHit = true;
            hitInfo.dst = dst;
            hitInfo.hitPoint = intersection;
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

vec3 RandomDirection(inout vec2 state){
    return normalize(vec3(randNorm(state), randNorm(state), randNorm(state)));
}

vec3 GetAmbientLight(Ray ray){
    float gradient = pow(smoothstep(0.0f,0.4f,float(ray.direction.y)), 0.35f);
    vec3 gradientC = mix(SkyColourHorizon, SkyColourZenith, gradient);
    float sun = pow(max(0, dot(ray.direction, -SunLightDirection) - SunThreshold), SunFocus) * SunIntensity;

    float groundToSkyT = smoothstep(-0.01f,0.0f,float(ray.direction.y));
    float sunMask = groundToSkyT >= 1 ? 1 : 0;
    return mix(GroundColor, gradientC, groundToSkyT) + sun * sunMask;
}

vec3 RandomDirectionHemsiphere(vec3 normal, inout vec2 state){
    vec3 dir = RandomDirection(state);
    return dir * sign(dot(dir,normal));
}

vec3 FullTrace(Ray ray, inout vec2 state){
    vec3 rayColor = vec3(1,1,1);
    vec3 rayLight = vec3(0,0,0);
    int numTriTests = 0;

    for(int i = 0; i < NumberOfBounces; i++){
        HitInfo hitInfo;
        hitInfo.didHit = false;

        HitInfo hitInfoSphere = RayAllSpheres(ray);
        HitInfo hitInfoMesh = RayAllMeshes(ray, numTriTests);

        if(hitInfoSphere.didHit){
            hitInfo = hitInfoSphere;
            if(hitInfoMesh.didHit && hitInfoMesh.dst < hitInfoSphere.dst){
                hitInfo = hitInfoMesh;
            }
        } else{
            if(hitInfoMesh.didHit) {
                hitInfo = hitInfoMesh;
            }
        }
        if(hitInfo.didHit){  
            ray.origin = hitInfo.hitPoint;

            vec3 diffuseDirection = normalize(hitInfo.normal + RandomDirection(state));
            vec3 specularDirection = reflect(ray.direction, hitInfo.normal);
            bool isSpecular = hitInfo.material.specularProbability.x >= (abs(rand(state)));
            ray.direction = mix(diffuseDirection, specularDirection, hitInfo.material.smoothness.x * int(isSpecular));

            vec3 emmision = hitInfo.material.emmisionColor * hitInfo.material.emmisionStrength.x;
            rayLight += emmision * rayColor;
            rayColor *= mix(hitInfo.material.diffuseColor, hitInfo.material.specularColor, int(isSpecular));
        } 
        else
        {
            rayLight += rayColor * GetAmbientLight(ray);
            break;
        }
    }
    float debugTests = numTriTests / 2000.0;
    vec3 debugColor = debugTests < 1 ? vec3(debugTests) : vec3(1,0,0);
    return DebugMode == 0 ? rayLight : debugColor;
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