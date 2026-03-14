#ifndef MESH_H
#define MESH_H

#include "handles.h"
#include <glad/glad.h>

typedef struct sMesh {
    MeshID id;
    /* add vbo, ebo, vao, etc. for rendering */
    GLuint vbo;
    GLuint ebo;
    GLuint vao;
    GLsizeiptr vbo_size;
    GLsizeiptr ebo_size;
    GLsizeiptr vao_size;
    void *user; /* user pointer for later extension */
} Mesh;

MeshID Tropic_createCubeMesh(TropicID engine_id);

#endif /* MESH_H */
