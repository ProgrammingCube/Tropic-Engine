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
uniform float time;

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

vec3 rippleWarp(vec3 point, vec3 halfExtents, float t)
{
    float minExtent = min(halfExtents.x, min(halfExtents.y, halfExtents.z));
    float amplitude = max(minExtent * 0.05, 0.04);
    vec3 warpedPoint = point;

    warpedPoint.x += sin(point.y * 4.8 + point.z * 2.1 + t * 4.9) * amplitude;
    warpedPoint.y += sin(point.z * 5.5 + point.x * 1.9 + t * 3.8) * amplitude * 0.75;
    warpedPoint.z += sin(point.x * 5.1 + point.y * 2.4 + t * 5.4) * amplitude;

    return warpedPoint;
}

float travelingRipple(vec3 point, float t)
{
    float wave = sin(point.x * 3.8 + point.y * 2.8 + point.z * 4.2 - t * 8.8);
    wave += sin(point.x * 7.2 - point.z * 6.0 + t * 6.1) * 0.6;
    return 0.85 + wave * 0.35;
}

float distanceToQuarterArc(vec3 point, vec3 center, int axisA, int axisB, int fixedAxis, float radius)
{
    vec3 local = point - center;
    vec2 arcPlane;
    vec2 endpointA;
    vec2 endpointB;
    vec2 radialDir;
    vec2 closestArcPoint;
    float radialLength;
    float arcDistance;
    float endpointDistanceA;
    float endpointDistanceB;

    if (axisA == 0 && axisB == 1) {
        arcPlane = local.xy;
    } else if (axisA == 0 && axisB == 2) {
        arcPlane = vec2(local.x, local.z);
    } else {
        arcPlane = local.yz;
    }

    endpointA = vec2(radius, 0.0);
    endpointB = vec2(0.0, radius);
    radialDir = max(arcPlane, vec2(0.0));
    radialLength = length(radialDir);
    closestArcPoint = radialLength > 0.0001 ? radialDir * (radius / radialLength) : endpointA;

    arcDistance = length(vec3(arcPlane - closestArcPoint, local[fixedAxis]));
    endpointDistanceA = length(vec3(arcPlane - endpointA, local[fixedAxis]));
    endpointDistanceB = length(vec3(arcPlane - endpointB, local[fixedAxis]));

    return min(arcDistance, min(endpointDistanceA, endpointDistanceB));
}

float distanceToRoundedInnerPrismFrame(vec3 point, vec3 halfExtents)
{
    vec3 foldedPoint = abs(point);
    vec3 edgeCenter = innerEdgeCenter(halfExtents);
    float minExtent = min(edgeCenter.x, min(edgeCenter.y, edgeCenter.z));
    float tubeRadius = tubeRadiusForBounds(halfExtents);
    float cornerRadius = min(max(tubeRadius * 0.95, minExtent * 0.16), minExtent * 0.75);
    vec3 straightLimit = max(edgeCenter - vec3(cornerRadius), vec3(0.0));
    float frameDistance = 1e20;

    frameDistance = min(frameDistance, distanceToSegment(foldedPoint, vec3(0.0, edgeCenter.y, edgeCenter.z), vec3(straightLimit.x, edgeCenter.y, edgeCenter.z)));
    frameDistance = min(frameDistance, distanceToSegment(foldedPoint, vec3(edgeCenter.x, 0.0, edgeCenter.z), vec3(edgeCenter.x, straightLimit.y, edgeCenter.z)));
    frameDistance = min(frameDistance, distanceToSegment(foldedPoint, vec3(edgeCenter.x, edgeCenter.y, 0.0), vec3(edgeCenter.x, edgeCenter.y, straightLimit.z)));

    frameDistance = min(frameDistance, distanceToQuarterArc(foldedPoint,
                                                           vec3(straightLimit.x, straightLimit.y, edgeCenter.z),
                                                           0,
                                                           1,
                                                           2,
                                                           cornerRadius));
    frameDistance = min(frameDistance, distanceToQuarterArc(foldedPoint,
                                                           vec3(straightLimit.x, edgeCenter.y, straightLimit.z),
                                                           0,
                                                           2,
                                                           1,
                                                           cornerRadius));
    frameDistance = min(frameDistance, distanceToQuarterArc(foldedPoint,
                                                           vec3(edgeCenter.x, straightLimit.y, straightLimit.z),
                                                           1,
                                                           2,
                                                           0,
                                                           cornerRadius));

    return frameDistance;
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
        vec3 warpedPoint = rippleWarp(samplePoint, halfExtents, time);
        float ripple = travelingRipple(warpedPoint, time);
        float edgeDistance = distanceToRoundedInnerPrismFrame(warpedPoint, halfExtents);
        float halo = exp(-2.1 * edgeDistance / max(tubeRadius, 0.0001)) * ripple;
        float bloom = exp(-0.78 * edgeDistance / max(tubeRadius, 0.0001)) * (0.9 + ripple * 0.45);
        float core = exp(-9.0 * edgeDistance / max(tubeRadius, 0.0001)) * (0.85 + ripple * 0.55);
        vec3 sampleColor = vec3(0.04, 0.30, 0.10) * bloom;
        sampleColor += vec3(0.10, 0.95, 0.30) * halo;
        sampleColor += vec3(0.75, 1.00, 0.18) * core;

        volumeColor += sampleColor * stepLength * 3.0 / max(tubeRadius, 0.05);
        glowAmount += (halo + bloom * 0.85) * stepLength * 2.0 / max(tubeRadius, 0.05);
        coreAmount += core * stepLength * 1.25 / max(tubeRadius, 0.05);
    }

    float fresnel = pow(1.0 - max(dot(norm, viewDir), 0.0), 2.2);
    vec3 shellColor = vec3(0.0, 0.01, 0.005);
    shellColor += objectColor * 0.012;
    shellColor += objectColor * diff * 0.015;
    shellColor += vec3(0.01, 0.08, 0.03) * fresnel * 0.35;

    vec3 neonColor = shellColor + volumeColor;
    neonColor += vec3(0.08, 0.26, 0.10) * clamp(glowAmount, 0.0, 1.8);
    neonColor += vec3(0.22, 0.24, 0.08) * clamp(coreAmount, 0.0, 1.35);
    neonColor = min(neonColor, vec3(1.0));

    float shellAlpha = 0.08 + fresnel * 0.07;
    float glowAlpha = clamp(glowAmount * 0.58 + coreAmount * 0.32, 0.0, 0.78);
    float platformAlpha = clamp(shellAlpha + glowAlpha, 0.12, 0.84);

    FragColor = vec4(mix(litColor, neonColor, clamp(neonAmount, 0.0, 1.0)), mix(1.0, platformAlpha, clamp(neonAmount, 0.0, 1.0)));
}
