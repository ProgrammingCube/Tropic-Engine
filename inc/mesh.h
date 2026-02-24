#ifndef MESH_H
#define MESH_H

#include <stdint.h>
#include <glad/glad.h>

typedef struct sMesh {
    uint32_t id;
    /* add vbo, ebo, vao, etc. for rendering */
    GLuint vbo;
    GLuint ebo;
    GLuint vao;
    GLsizeiptr vbo_size;
    GLsizeiptr ebo_size;
    GLsizeiptr vao_size;
    void *user; /* user pointer for later extension */
} Mesh;

#endif /* MESH_H */
