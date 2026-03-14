#version 330 core
in vec3 FragPos;
in vec3 Normal;
in vec3 LocalPos;

out vec4 FragColor;

uniform vec3 lightPos;
uniform vec3 objectColor;
uniform vec3 ambientColor;
uniform vec3 objectScale;
uniform float neonAmount;

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

float distanceToInnerPrismEdges(vec3 point, vec3 halfExtents)
{
    float minExtent = min(halfExtents.x, min(halfExtents.y, halfExtents.z));
    float tubeRadius = max(minExtent * 0.18, 0.08);
    float inset = min(tubeRadius * 0.8, minExtent * 0.45);
    vec3 edgeCenter = max(halfExtents - vec3(inset), vec3(0.0));

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

void main()
{
    vec3 norm = normalize(Normal);
    vec3 lightDir = normalize(lightPos - FragPos);
    float diff = max(dot(norm, lightDir), 0.0);

    vec3 ambient = ambientColor * objectColor;
    vec3 diffuse = diff * objectColor;
    vec3 litColor = ambient + diffuse;

    vec3 halfExtents = max(abs(objectScale), vec3(0.001));
    vec3 platformPoint = LocalPos * halfExtents;
    float minExtent = min(halfExtents.x, min(halfExtents.y, halfExtents.z));
    float tubeRadius = max(minExtent * 0.18, 0.08);
    float edgeDistance = distanceToInnerPrismEdges(platformPoint, halfExtents);

    float tubeCore = 1.0 - smoothstep(0.0, tubeRadius * 0.45, edgeDistance);
    float tubeGlow = 1.0 - smoothstep(tubeRadius * 0.18, tubeRadius * 1.9, edgeDistance);

    vec3 baseBody = vec3(0.01, 0.05, 0.02) + objectColor * (0.10 + diff * 0.10);
    vec3 glowColor = vec3(0.10, 0.92, 0.30) * tubeGlow * 1.25;
    vec3 coreColor = vec3(0.92, 1.00, 0.28) * tubeCore * 1.35;
    vec3 neonColor = baseBody + glowColor + coreColor;

    FragColor = vec4(mix(litColor, neonColor, clamp(neonAmount, 0.0, 1.0)), 1.0);
}
