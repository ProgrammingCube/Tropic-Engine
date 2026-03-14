#include "tropic.h"
#include "primitives.h"

MeshID Tropic_createCubeMesh(TropicID engine_id)
{
    Tropic *self = Tropic_getById(engine_id);
    Scene *scene = Tropic_getCurrentScenePtr(self);
    Mesh mesh = { 0 };
    MeshID mesh_id;

    if (!self || !self->window || !scene) return 0;

    glfwMakeContextCurrent(self->window);

    glGenVertexArrays(1, &mesh.vao);
    glGenBuffers(1, &mesh.vbo);
    glGenBuffers(1, &mesh.ebo);

    if (mesh.vao == 0 || mesh.vbo == 0 || mesh.ebo == 0)
    {
        if (mesh.vbo != 0) glDeleteBuffers(1, &mesh.vbo);
        if (mesh.ebo != 0) glDeleteBuffers(1, &mesh.ebo);
        if (mesh.vao != 0) glDeleteVertexArrays(1, &mesh.vao);
        return 0;
    }

    glBindVertexArray(mesh.vao);
    glBindBuffer(GL_ARRAY_BUFFER, mesh.vbo);
    glBufferData(GL_ARRAY_BUFFER, sizeof(cube_verticies), cube_verticies, GL_STATIC_DRAW);
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, mesh.ebo);
    glBufferData(GL_ELEMENT_ARRAY_BUFFER, sizeof(cube_indices), cube_indices, GL_STATIC_DRAW);
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, CUBE_VERTEX_STRIDE, (void*)0);
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, CUBE_VERTEX_STRIDE, (void*)(3 * sizeof(GLfloat)));
    glEnableVertexAttribArray(1);
    glBindVertexArray(0);

    mesh.vbo_size = sizeof(cube_verticies);
    mesh.ebo_size = sizeof(cube_indices);
    mesh.vao_size = 1;

    mesh_id = Tropic_newMesh(engine_id, &mesh);
    if (mesh_id == 0)
    {
        glDeleteBuffers(1, &mesh.vbo);
        glDeleteBuffers(1, &mesh.ebo);
        glDeleteVertexArrays(1, &mesh.vao);
    }

    return mesh_id;
}
