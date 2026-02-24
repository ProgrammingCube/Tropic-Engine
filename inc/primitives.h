#ifndef PRIMITIVES_H
#define PRIMITIVES_H

#include <glad/glad.h>

static const GLfloat cube_verticies[] = {
    -1.0f, -1.0f, -1.0f,  0.0f,  0.0f, -1.0f,
     1.0f, -1.0f, -1.0f,  0.0f,  0.0f, -1.0f,
     1.0f,  1.0f, -1.0f,  0.0f,  0.0f, -1.0f,
    -1.0f,  1.0f, -1.0f,  0.0f,  0.0f, -1.0f,

    -1.0f, -1.0f,  1.0f,  0.0f,  0.0f,  1.0f,
     1.0f, -1.0f,  1.0f,  0.0f,  0.0f,  1.0f,
     1.0f,  1.0f,  1.0f,  0.0f,  0.0f,  1.0f,
    -1.0f,  1.0f,  1.0f,  0.0f,  0.0f,  1.0f,

    -1.0f, -1.0f, -1.0f, -1.0f,  0.0f,  0.0f,
    -1.0f,  1.0f, -1.0f, -1.0f,  0.0f,  0.0f,
    -1.0f,  1.0f,  1.0f, -1.0f,  0.0f,  0.0f,
    -1.0f, -1.0f,  1.0f, -1.0f,  0.0f,  0.0f,

     1.0f, -1.0f, -1.0f,  1.0f,  0.0f,  0.0f,
     1.0f,  1.0f, -1.0f,  1.0f,  0.0f,  0.0f,
     1.0f,  1.0f,  1.0f,  1.0f,  0.0f,  0.0f,
     1.0f, -1.0f,  1.0f,  1.0f,  0.0f,  0.0f,

    -1.0f, -1.0f, -1.0f,  0.0f, -1.0f,  0.0f,
    -1.0f, -1.0f,  1.0f,  0.0f, -1.0f,  0.0f,
     1.0f, -1.0f,  1.0f,  0.0f, -1.0f,  0.0f,
     1.0f, -1.0f, -1.0f,  0.0f, -1.0f,  0.0f,

    -1.0f,  1.0f, -1.0f,  0.0f,  1.0f,  0.0f,
    -1.0f,  1.0f,  1.0f,  0.0f,  1.0f,  0.0f,
     1.0f,  1.0f,  1.0f,  0.0f,  1.0f,  0.0f,
     1.0f,  1.0f, -1.0f,  0.0f,  1.0f,  0.0f,
};

static const GLuint cube_indices[] = {
    0, 1, 2, 2, 3, 0,
    4, 5, 6, 6, 7, 4,
    8, 9, 10, 10, 11, 8,
    12, 13, 14, 14, 15, 12,
    16, 17, 18, 18, 19, 16,
    20, 21, 22, 22, 23, 20,
};

#define CUBE_VERTEX_STRIDE (6 * sizeof(GLfloat))
#define CUBE_VERTEX_COUNT 24
#define CUBE_INDEX_COUNT 36

#endif