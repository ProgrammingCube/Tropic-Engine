#include "shader.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static char* shader_read_file(const char *path)
{
    if (!path) return NULL;
    FILE *file = fopen(path, "rb");
    if (!file) return NULL;

    if (fseek(file, 0, SEEK_END) != 0) {
        fclose(file);
        return NULL;
    }

    long size = ftell(file);
    if (size < 0) {
        fclose(file);
        return NULL;
    }
    rewind(file);

    char *buffer = (char*)malloc((size_t)size + 1);
    if (!buffer) {
        fclose(file);
        return NULL;
    }

    size_t read_bytes = fread(buffer, 1, (size_t)size, file);
    fclose(file);
    if (read_bytes != (size_t)size) {
        free(buffer);
        return NULL;
    }

    buffer[size] = '\0';
    return buffer;
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

    char *vertex_source = shader_read_file(vertex_path);
    char *fragment_source = shader_read_file(fragment_path);
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
