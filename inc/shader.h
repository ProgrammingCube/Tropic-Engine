#ifndef SHADER_H
#define SHADER_H

#include <stdbool.h>
#include <stddef.h>
#include <cglm/cglm.h>
#include "handles.h"
#include <glad/glad.h>

typedef struct sShader {
    ShaderID id;
    GLuint program;
    void *user;
} Shader;

bool shader_load_from_files(Shader *out_shader, const char *vertex_path, const char *fragment_path);
void shader_destroy(Shader *shader);
void shader_use(const Shader *shader);
GLint shader_get_uniform_location(const Shader *shader, const char *name);

bool Tropic_createShaderFromFiles(TropicID engine_id,
                                  const char *vertex_path,
                                  const char *fragment_path,
                                  ShaderID *out_shader_id);
bool Tropic_createShaderFromFileCandidates(TropicID engine_id,
                                           const char *const *vertex_paths,
                                           const char *const *fragment_paths,
                                           size_t candidate_count,
                                           ShaderID *out_shader_id);
bool Tropic_setShaderUniformFloat(TropicID engine_id,
                                  ShaderID shader_id,
                                  const char *name,
                                  float value);
bool Tropic_setShaderUniformVec3(TropicID engine_id,
                                 ShaderID shader_id,
                                 const char *name,
                                 vec3 value);

#endif
