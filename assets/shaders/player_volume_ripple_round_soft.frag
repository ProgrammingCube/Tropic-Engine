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
uniform float brightnessScale;
uniform float alphaScale;
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

float sdRoundedBox(vec3 point, vec3 halfExtents, float radius)
{
    vec3 q = abs(point) - halfExtents;
    return length(max(q, vec3(0.0))) + min(max(q.x, max(q.y, q.z)), 0.0) - radius;
}

float tubeRadiusForBounds(vec3 halfExtents)
{
    float minExtent = min(halfExtents.x, min(halfExtents.y, halfExtents.z));
    return max(minExtent * 0.10, 0.045);
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
    float amplitude = max(minExtent * 0.015, 0.008);
    vec3 warpedPoint = point;

    warpedPoint.x += sin(point.y * 4.8 + point.z * 2.1 + t * 3.2) * amplitude;
    warpedPoint.y += sin(point.z * 5.5 + point.x * 1.9 + t * 2.6) * amplitude * 0.45;
    warpedPoint.z += sin(point.x * 5.1 + point.y * 2.4 + t * 3.5) * amplitude;

    return warpedPoint;
}

float travelingRipple(vec3 point, float t)
{
    float wave = sin(point.x * 3.8 + point.y * 2.8 + point.z * 4.2 - t * 8.8);
    wave += sin(point.x * 7.2 - point.z * 6.0 + t * 6.1) * 0.25;
    return 0.96 + wave * 0.12;
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
    float visualTubeRadius = max(tubeRadius * 0.38, 0.024);
    float minExtent = min(halfExtents.x, min(halfExtents.y, halfExtents.z));
    float outerCornerRadius = min(max(tubeRadius * 1.05, minExtent * 0.20), minExtent * 0.70);
    vec3 cameraLocal = (inverseModel * vec4(cameraPos, 1.0)).xyz * halfExtents;
    vec3 surfacePoint = LocalPos * halfExtents;
    vec3 rayDir = normalize(surfacePoint - cameraLocal);
    vec3 entryPoint = surfacePoint + rayDir * (visualTubeRadius * 0.08);
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
        float halo = exp(-4.6 * edgeDistance / max(visualTubeRadius, 0.0001)) * ripple;
        float bloom = exp(-1.85 * edgeDistance / max(visualTubeRadius, 0.0001)) * (0.8 + ripple * 0.20);
        float core = exp(-13.5 * edgeDistance / max(visualTubeRadius, 0.0001)) * (0.92 + ripple * 0.18);
        vec3 sampleColor = vec3(0.006, 0.028, 0.090) * bloom;
        sampleColor += vec3(0.030, 0.220, 0.520) * halo;
        sampleColor += vec3(0.250, 0.650, 1.000) * core;

        volumeColor += sampleColor * stepLength * 1.15 / max(visualTubeRadius, 0.03);
        glowAmount += (halo + bloom * 0.50) * stepLength * 0.90 / max(visualTubeRadius, 0.03);
        coreAmount += core * stepLength * 0.70 / max(visualTubeRadius, 0.03);
    }

    float outerCornerDistance = max(sdRoundedBox(surfacePoint,
                                                 max(halfExtents - vec3(outerCornerRadius), vec3(0.001)),
                                                 outerCornerRadius),
                                    0.0);
    float outerCornerSoften = smoothstep(0.0, outerCornerRadius * 0.95, outerCornerDistance);

    float fresnel = pow(1.0 - max(dot(norm, viewDir), 0.0), 2.2);
    vec3 shellColor = vec3(0.0, 0.001, 0.005);
    shellColor += objectColor * 0.004;
    shellColor += objectColor * diff * 0.006;
    shellColor += vec3(0.03, 0.12, 0.38) * fresnel * 0.16;
    shellColor *= mix(1.0, 0.10, outerCornerSoften);

    vec3 neonColor = shellColor + volumeColor;
    neonColor += vec3(0.030, 0.110, 0.320) * clamp(glowAmount, 0.0, 1.0);
    neonColor += vec3(0.100, 0.280, 0.620) * clamp(coreAmount, 0.0, 0.8);
    neonColor += vec3(0.012, 0.035, 0.080) * outerCornerSoften;
    neonColor *= max(brightnessScale, 0.0);
    neonColor = min(neonColor, vec3(1.0));

    float shellAlpha = (0.020 + fresnel * 0.032) * mix(1.0, 0.08, outerCornerSoften);
    float glowAlpha = clamp(glowAmount * 0.26 + coreAmount * 0.16 + outerCornerSoften * 0.04, 0.0, 0.34);
    float platformAlpha = clamp(shellAlpha + glowAlpha, 0.0, 0.40);
    float finalAlpha = clamp(platformAlpha * max(alphaScale, 0.0), 0.0, 1.0);
    vec3 finalColor = mix(litColor, neonColor, clamp(neonAmount, 0.0, 1.0));

    FragColor = vec4(finalColor, finalAlpha * clamp(neonAmount, 0.0, 1.0));
}
