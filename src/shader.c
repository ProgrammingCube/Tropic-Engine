#include "shader.h"
#include "tropic.h"
#include "utils.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static bool _shader_file_exists(const char *path)
{
    FILE *file;

    if (!path) return false;

    file = fopen(path, "rb");
    if (!file) return false;

    fclose(file);
    return true;
}

static bool shader_compile(GLuint shader, const char *source, const char *label)
{
    glShaderSource(shader, 1, &source, NULL);
    glCompileShader(shader);

    GLint success = GL_FALSE;
    glGetShaderiv(shader, GL_COMPILE_STATUS, &success);
    if (success == GL_TRUE) return true;

    char log[1024];
    GLsizei length = 0;
    glGetShaderInfoLog(shader, sizeof(log), &length, log);
    fprintf(stderr, "Shader compile failed (%s): %.*s\n", label ? label : "unknown", (int)length, log);
    return false;
}

bool shader_load_from_files(Shader *out_shader, const char *vertex_path, const char *fragment_path)
{
    if (!out_shader || !vertex_path || !fragment_path) return false;

    char *vertex_source = read_file_to_buf(vertex_path);
    char *fragment_source = read_file_to_buf(fragment_path);
    if (!vertex_source || !fragment_source) {
        free(vertex_source);
        free(fragment_source);
        return false;
    }

    GLuint vertex_shader = glCreateShader(GL_VERTEX_SHADER);
    GLuint fragment_shader = glCreateShader(GL_FRAGMENT_SHADER);

    bool compiled = shader_compile(vertex_shader, vertex_source, vertex_path) &&
                    shader_compile(fragment_shader, fragment_source, fragment_path);

    free(vertex_source);
    free(fragment_source);

    if (!compiled) {
        glDeleteShader(vertex_shader);
        glDeleteShader(fragment_shader);
        return false;
    }

    GLuint program = glCreateProgram();
    glAttachShader(program, vertex_shader);
    glAttachShader(program, fragment_shader);
    glLinkProgram(program);

    GLint success = GL_FALSE;
    glGetProgramiv(program, GL_LINK_STATUS, &success);
    glDeleteShader(vertex_shader);
    glDeleteShader(fragment_shader);

    if (success != GL_TRUE) {
        char log[1024];
        GLsizei length = 0;
        glGetProgramInfoLog(program, sizeof(log), &length, log);
        fprintf(stderr, "Shader link failed: %.*s\n", (int)length, log);
        glDeleteProgram(program);
        return false;
    }

    out_shader->program = program;
    return true;
}

void shader_destroy(Shader *shader)
{
    if (!shader) return;
    if (shader->program != 0) {
        glDeleteProgram(shader->program);
        shader->program = 0;
    }
}

void shader_use(const Shader *shader)
{
    if (!shader || shader->program == 0) return;
    glUseProgram(shader->program);
}

GLint shader_get_uniform_location(const Shader *shader, const char *name)
{
    if (!shader || shader->program == 0 || !name) return -1;
    return glGetUniformLocation(shader->program, name);
}

bool Tropic_createShaderFromFiles(TropicID engine_id,
                                  const char *vertex_path,
                                  const char *fragment_path,
                                  ShaderID *out_shader_id)
{
    Shader shader = {0};
    ShaderID shader_id;

    if (!out_shader_id || !vertex_path || !fragment_path) return false;

    *out_shader_id = 0;

    if (!shader_load_from_files(&shader, vertex_path, fragment_path)) {
        return false;
    }

    shader_id = Tropic_newShader(engine_id, &shader);
    if (shader_id == 0) {
        shader_destroy(&shader);
        return false;
    }

    *out_shader_id = shader_id;
    return true;
}

bool Tropic_createShaderFromFileCandidates(TropicID engine_id,
                                           const char *const *vertex_paths,
                                           const char *const *fragment_paths,
                                           size_t candidate_count,
                                           ShaderID *out_shader_id)
{
    if (!out_shader_id || !vertex_paths || !fragment_paths || candidate_count == 0) return false;

    *out_shader_id = 0;

    for (size_t i = 0; i < candidate_count; ++i) {
        if (!_shader_file_exists(vertex_paths[i]) || !_shader_file_exists(fragment_paths[i])) {
            continue;
        }

        if (Tropic_createShaderFromFiles(engine_id, vertex_paths[i], fragment_paths[i], out_shader_id)) {
            return true;
        }
    }

    return false;
}

bool Tropic_setShaderUniformFloat(TropicID engine_id,
                                  ShaderID shader_id,
                                  const char *name,
                                  float value)
{
    Shader *shader = Tropic_getShader(engine_id, shader_id);
    GLint uniform_location;

    if (!shader || !name) return false;

    uniform_location = shader_get_uniform_location(shader, name);
    if (uniform_location < 0) return false;

    glUniform1f(uniform_location, value);
    return true;
}

bool Tropic_setShaderUniformVec3(TropicID engine_id,
                                 ShaderID shader_id,
                                 const char *name,
                                 vec3 value)
{
    Shader *shader = Tropic_getShader(engine_id, shader_id);
    GLint uniform_location;

    if (!shader || !name) return false;

    uniform_location = shader_get_uniform_location(shader, name);
    if (uniform_location < 0) return false;

    glUniform3fv(uniform_location, 1, value);
    return true;
}
