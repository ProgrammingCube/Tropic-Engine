#version 330 core
in vec3 FragPos;
in vec3 Normal;
in vec3 LocalPos;

out vec4 FragColor;

uniform vec3 lightPos;
uniform vec3 objectColor;
uniform vec3 ambientColor;
uniform vec3 objectScale;
uniform vec3 cameraPos;
uniform mat4 inverseModel;
uniform float neonAmount;

const int VOLUME_STEPS = 64;

float distanceToSegment(vec3 point, vec3 startPoint, vec3 endPoint)
{
    vec3 segment = endPoint - startPoint;
    float lengthSquared = dot(segment, segment);
    if (lengthSquared <= 0.000001) {
        return distance(point, startPoint);
    }

    float t = clamp(dot(point - startPoint, segment) / lengthSquared, 0.0, 1.0);
    vec3 closestPoint = startPoint + segment * t;
    return distance(point, closestPoint);
}

float componentInverse(float value)
{
    if (abs(value) > 0.0001) {
        return 1.0 / value;
    }

    return value >= 0.0 ? 100000.0 : -100000.0;
}

float tubeRadiusForBounds(vec3 halfExtents)
{
    float minExtent = min(halfExtents.x, min(halfExtents.y, halfExtents.z));
    return max(minExtent * 0.22, 0.12);
}

vec3 innerEdgeCenter(vec3 halfExtents)
{
    float minExtent = min(halfExtents.x, min(halfExtents.y, halfExtents.z));
    float tubeRadius = tubeRadiusForBounds(halfExtents);
    float inset = min(max(tubeRadius * 1.10, minExtent * 0.10), minExtent * 0.34);
    return max(halfExtents - vec3(inset), vec3(0.0));
}

float distanceToInnerPrismEdges(vec3 point, vec3 halfExtents)
{
    vec3 edgeCenter = innerEdgeCenter(halfExtents);
    float edgeDistance = 1e20;

    edgeDistance = min(edgeDistance, distanceToSegment(point, vec3(-edgeCenter.x, -edgeCenter.y, -edgeCenter.z), vec3( edgeCenter.x, -edgeCenter.y, -edgeCenter.z)));
    edgeDistance = min(edgeDistance, distanceToSegment(point, vec3(-edgeCenter.x, -edgeCenter.y,  edgeCenter.z), vec3( edgeCenter.x, -edgeCenter.y,  edgeCenter.z)));
    edgeDistance = min(edgeDistance, distanceToSegment(point, vec3(-edgeCenter.x,  edgeCenter.y, -edgeCenter.z), vec3( edgeCenter.x,  edgeCenter.y, -edgeCenter.z)));
    edgeDistance = min(edgeDistance, distanceToSegment(point, vec3(-edgeCenter.x,  edgeCenter.y,  edgeCenter.z), vec3( edgeCenter.x,  edgeCenter.y,  edgeCenter.z)));

    edgeDistance = min(edgeDistance, distanceToSegment(point, vec3(-edgeCenter.x, -edgeCenter.y, -edgeCenter.z), vec3(-edgeCenter.x,  edgeCenter.y, -edgeCenter.z)));
    edgeDistance = min(edgeDistance, distanceToSegment(point, vec3(-edgeCenter.x, -edgeCenter.y,  edgeCenter.z), vec3(-edgeCenter.x,  edgeCenter.y,  edgeCenter.z)));
    edgeDistance = min(edgeDistance, distanceToSegment(point, vec3( edgeCenter.x, -edgeCenter.y, -edgeCenter.z), vec3( edgeCenter.x,  edgeCenter.y, -edgeCenter.z)));
    edgeDistance = min(edgeDistance, distanceToSegment(point, vec3( edgeCenter.x, -edgeCenter.y,  edgeCenter.z), vec3( edgeCenter.x,  edgeCenter.y,  edgeCenter.z)));

    edgeDistance = min(edgeDistance, distanceToSegment(point, vec3(-edgeCenter.x, -edgeCenter.y, -edgeCenter.z), vec3(-edgeCenter.x, -edgeCenter.y,  edgeCenter.z)));
    edgeDistance = min(edgeDistance, distanceToSegment(point, vec3(-edgeCenter.x,  edgeCenter.y, -edgeCenter.z), vec3(-edgeCenter.x,  edgeCenter.y,  edgeCenter.z)));
    edgeDistance = min(edgeDistance, distanceToSegment(point, vec3( edgeCenter.x, -edgeCenter.y, -edgeCenter.z), vec3( edgeCenter.x, -edgeCenter.y,  edgeCenter.z)));
    edgeDistance = min(edgeDistance, distanceToSegment(point, vec3( edgeCenter.x,  edgeCenter.y, -edgeCenter.z), vec3( edgeCenter.x,  edgeCenter.y,  edgeCenter.z)));

    return edgeDistance;
}

float rayExitDistance(vec3 origin, vec3 direction, vec3 halfExtents)
{
    vec3 invDir = vec3(componentInverse(direction.x), componentInverse(direction.y), componentInverse(direction.z));
    vec3 t0 = (-halfExtents - origin) * invDir;
    vec3 t1 = ( halfExtents - origin) * invDir;
    vec3 tFar = max(t0, t1);
    return min(tFar.x, min(tFar.y, tFar.z));
}

void main()
{
    vec3 norm = normalize(Normal);
    vec3 lightDir = normalize(lightPos - FragPos);
    vec3 viewDir = normalize(cameraPos - FragPos);
    float diff = max(dot(norm, lightDir), 0.0);

    vec3 ambient = ambientColor * objectColor;
    vec3 diffuse = diff * objectColor;
    vec3 litColor = ambient + diffuse;

    vec3 halfExtents = max(abs(objectScale), vec3(0.001));
    float tubeRadius = tubeRadiusForBounds(halfExtents);
    vec3 cameraLocal = (inverseModel * vec4(cameraPos, 1.0)).xyz * halfExtents;
    vec3 surfacePoint = LocalPos * halfExtents;
    vec3 rayDir = normalize(surfacePoint - cameraLocal);
    vec3 entryPoint = surfacePoint + rayDir * (tubeRadius * 0.12);
    float marchDistance = max(rayExitDistance(entryPoint, rayDir, halfExtents), 0.0);
    float stepLength = marchDistance / float(VOLUME_STEPS);

    vec3 volumeColor = vec3(0.0);
    float glowAmount = 0.0;
    float coreAmount = 0.0;

    for (int i = 0; i < VOLUME_STEPS; ++i)
    {
        float sampleT = stepLength * (float(i) + 0.5);
        vec3 samplePoint = entryPoint + rayDir * sampleT;
        float edgeDistance = distanceToInnerPrismEdges(samplePoint, halfExtents);
        float halo = exp(-2.2 * edgeDistance / max(tubeRadius, 0.0001));
        float bloom = exp(-0.85 * edgeDistance / max(tubeRadius, 0.0001));
        float core = exp(-9.5 * edgeDistance / max(tubeRadius, 0.0001));
        vec3 sampleColor = vec3(0.04, 0.30, 0.10) * bloom;
        sampleColor += vec3(0.10, 0.95, 0.30) * halo;
        sampleColor += vec3(0.75, 1.00, 0.18) * core;

        volumeColor += sampleColor * stepLength * 2.8 / max(tubeRadius, 0.05);
        glowAmount += (halo + bloom * 0.8) * stepLength * 1.8 / max(tubeRadius, 0.05);
        coreAmount += core * stepLength * 1.2 / max(tubeRadius, 0.05);
    }

    float fresnel = pow(1.0 - max(dot(norm, viewDir), 0.0), 2.2);
    vec3 shellColor = vec3(0.0, 0.01, 0.005);
    shellColor += objectColor * 0.012;
    shellColor += objectColor * diff * 0.015;
    shellColor += vec3(0.01, 0.08, 0.03) * fresnel * 0.35;

    vec3 neonColor = shellColor + volumeColor;
    neonColor += vec3(0.08, 0.26, 0.10) * clamp(glowAmount, 0.0, 1.6);
    neonColor += vec3(0.22, 0.24, 0.08) * clamp(coreAmount, 0.0, 1.2);
    neonColor = min(neonColor, vec3(1.0));

    float shellAlpha = 0.08 + fresnel * 0.07;
    float glowAlpha = clamp(glowAmount * 0.55 + coreAmount * 0.30, 0.0, 0.72);
    float platformAlpha = clamp(shellAlpha + glowAlpha, 0.12, 0.82);

    FragColor = vec4(mix(litColor, neonColor, clamp(neonAmount, 0.0, 1.0)), mix(1.0, platformAlpha, clamp(neonAmount, 0.0, 1.0)));
}
