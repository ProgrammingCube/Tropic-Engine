#ifndef SHADER_H
#define SHADER_H

#include <stdbool.h>
#include <stdint.h>
#include <glad/glad.h>

typedef struct sShader {
    uint32_t id;
    GLuint program;
    void *user;
} Shader;

bool shader_load_from_files(Shader *out_shader, const char *vertex_path, const char *fragment_path);
void shader_destroy(Shader *shader);
void shader_use(const Shader *shader);
GLint shader_get_uniform_location(const Shader *shader, const char *name);

#endif
