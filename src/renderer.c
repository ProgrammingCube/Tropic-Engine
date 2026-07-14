#include "tropic.h"
#include "renderer.h"

#include <cglm/cglm.h>
#include <stdlib.h>
#include <string.h>
#include <vector.h>

typedef struct sRendererDebugVertex
{
    float position[3];
    float color[3];
} RendererDebugVertex;

static GLuint _renderer_debug_line_program = 0;
static GLuint _renderer_debug_line_vao = 0;
static GLuint _renderer_debug_line_vbo = 0;

void Tropic_enableBeatGridDebug(TropicID engine_id, bool enabled)
{
    Tropic *self = Tropic_getById(engine_id);

    if (!self) return;
    self->beat_grid_debug_enabled = enabled;
}

bool Tropic_isBeatGridDebugEnabled(TropicID engine_id)
{
    Tropic *self = Tropic_getById(engine_id);

    if (!self) return false;
    return self->beat_grid_debug_enabled;
}

static bool _Renderer_compileDebugShader(GLuint shader, const char *source)
{
    GLint success = GL_FALSE;

    glShaderSource(shader, 1, &source, NULL);
    glCompileShader(shader);
    glGetShaderiv(shader, GL_COMPILE_STATUS, &success);
    if (success == GL_TRUE) return true;

    glDeleteShader(shader);
    return false;
}

static bool _Renderer_ensureDebugLineResources(void)
{
    static const char *vertex_source =
        "#version 330 core\n"
        "layout (location = 0) in vec3 aPos;\n"
        "layout (location = 1) in vec3 aColor;\n"
        "uniform mat4 view;\n"
        "uniform mat4 projection;\n"
        "out vec3 vColor;\n"
        "void main()\n"
        "{\n"
        "    vColor = aColor;\n"
        "    gl_Position = projection * view * vec4(aPos, 1.0);\n"
        "}\n";
    static const char *fragment_source =
        "#version 330 core\n"
        "in vec3 vColor;\n"
        "out vec4 FragColor;\n"
        "void main()\n"
        "{\n"
        "    FragColor = vec4(vColor, 1.0);\n"
        "}\n";
    GLuint vertex_shader;
    GLuint fragment_shader;
    GLint success = GL_FALSE;

    if (_renderer_debug_line_program != 0 &&
        _renderer_debug_line_vao != 0 &&
        _renderer_debug_line_vbo != 0) {
        return true;
    }

    vertex_shader = glCreateShader(GL_VERTEX_SHADER);
    fragment_shader = glCreateShader(GL_FRAGMENT_SHADER);
    if (!_Renderer_compileDebugShader(vertex_shader, vertex_source) ||
        !_Renderer_compileDebugShader(fragment_shader, fragment_source)) {
        if (vertex_shader != 0) glDeleteShader(vertex_shader);
        if (fragment_shader != 0) glDeleteShader(fragment_shader);
        return false;
    }

    _renderer_debug_line_program = glCreateProgram();
    glAttachShader(_renderer_debug_line_program, vertex_shader);
    glAttachShader(_renderer_debug_line_program, fragment_shader);
    glLinkProgram(_renderer_debug_line_program);
    glGetProgramiv(_renderer_debug_line_program, GL_LINK_STATUS, &success);
    glDeleteShader(vertex_shader);
    glDeleteShader(fragment_shader);
    if (success != GL_TRUE) {
        glDeleteProgram(_renderer_debug_line_program);
        _renderer_debug_line_program = 0;
        return false;
    }

    glGenVertexArrays(1, &_renderer_debug_line_vao);
    glGenBuffers(1, &_renderer_debug_line_vbo);
    if (_renderer_debug_line_vao == 0 || _renderer_debug_line_vbo == 0) return false;

    glBindVertexArray(_renderer_debug_line_vao);
    glBindBuffer(GL_ARRAY_BUFFER, _renderer_debug_line_vbo);
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, sizeof(RendererDebugVertex), (const void*)0);
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, sizeof(RendererDebugVertex), (const void*)(sizeof(float) * 3));
    glEnableVertexAttribArray(1);
    glBindVertexArray(0);
    glBindBuffer(GL_ARRAY_BUFFER, 0);
    return true;
}

static void _Renderer_pushDebugLine(RendererDebugVertex *vertices,
                                    size_t *vertex_count,
                                    size_t max_vertices,
                                    vec3 start,
                                    vec3 end,
                                    float r,
                                    float g,
                                    float b)
{
    if (!vertices || !vertex_count || !start || !end) return;
    if ((*vertex_count + 2) > max_vertices) return;

    memcpy(vertices[*vertex_count].position, start, sizeof(vec3));
    vertices[*vertex_count].color[0] = r;
    vertices[*vertex_count].color[1] = g;
    vertices[*vertex_count].color[2] = b;
    (*vertex_count)++;
    memcpy(vertices[*vertex_count].position, end, sizeof(vec3));
    vertices[*vertex_count].color[0] = r;
    vertices[*vertex_count].color[1] = g;
    vertices[*vertex_count].color[2] = b;
    (*vertex_count)++;
}

static void _Renderer_drawBeatGridDebug(TropicID engine_id,
                                        Scene *scene,
                                        mat4 view,
                                        mat4 projection)
{
    enum { MAX_DEBUG_VERTICES = 1024 };
    RendererDebugVertex vertices[MAX_DEBUG_VERTICES];
    size_t vertex_count = 0;
    TropicTrackPlacement placement = {0};
    const int beat_start = 0;
    const int beat_end = 40;
    const int lateral_cells = 4;
    const float axis_length = 3.0f;

    if (!scene || !Tropic_isBeatGridDebugEnabled(engine_id)) return;
    if (!_Renderer_ensureDebugLineResources()) return;

    placement.space = TROPIC_PLACEMENT_SPACE_TRACK;
    placement.snap_x = true;
    placement.snap_y = true;

    for (int beat = beat_start; beat <= beat_end; ++beat) {
        vec3 start;
        vec3 end;

        placement.time.beat = beat;
        placement.time.substep = 0;
        placement.track_y = 0.0f;
        placement.track_x = -(float)lateral_cells * scene->beat_grid.snap_unit_x;
        if (!Tropic_resolvePlacementPosition(engine_id, scene->id, &placement, start)) continue;
        placement.track_x = (float)lateral_cells * scene->beat_grid.snap_unit_x;
        if (!Tropic_resolvePlacementPosition(engine_id, scene->id, &placement, end)) continue;
        _Renderer_pushDebugLine(vertices, &vertex_count, MAX_DEBUG_VERTICES, start, end, 0.20f, 0.55f, 1.00f);
    }

    for (int lane = -lateral_cells; lane <= lateral_cells; ++lane) {
        vec3 start;
        vec3 end;

        placement.track_x = (float)lane * scene->beat_grid.snap_unit_x;
        placement.track_y = 0.0f;
        placement.time.beat = beat_start;
        placement.time.substep = 0;
        if (!Tropic_resolvePlacementPosition(engine_id, scene->id, &placement, start)) continue;
        placement.time.beat = beat_end;
        if (!Tropic_resolvePlacementPosition(engine_id, scene->id, &placement, end)) continue;
        _Renderer_pushDebugLine(vertices, &vertex_count, MAX_DEBUG_VERTICES, start, end, 0.12f, 0.25f, 0.45f);
    }

    {
        vec3 origin;
        vec3 axis_end;
        TropicTrackFrame frame = scene->base_track_frame;

        glm_vec3_copy(frame.origin, origin);
        glm_vec3_scale(frame.right, axis_length, axis_end);
        glm_vec3_add(origin, axis_end, axis_end);
        _Renderer_pushDebugLine(vertices, &vertex_count, MAX_DEBUG_VERTICES, origin, axis_end, 1.0f, 0.25f, 0.25f);
        glm_vec3_scale(frame.up, axis_length, axis_end);
        glm_vec3_add(origin, axis_end, axis_end);
        _Renderer_pushDebugLine(vertices, &vertex_count, MAX_DEBUG_VERTICES, origin, axis_end, 0.25f, 1.0f, 0.25f);
        glm_vec3_scale(frame.forward, axis_length, axis_end);
        glm_vec3_add(origin, axis_end, axis_end);
        _Renderer_pushDebugLine(vertices, &vertex_count, MAX_DEBUG_VERTICES, origin, axis_end, 0.25f, 0.70f, 1.0f);
    }

    if (scene->track_anchors) {
        for (size_t i = 0; i < vector_size(scene->track_anchors); ++i) {
            TropicTrackAnchor *anchor = &scene->track_anchors[i];
            vec3 pivot;
            vec3 marker_end;

            placement.time = anchor->start_time;
            placement.track_x = anchor->pivot_x;
            placement.track_y = anchor->pivot_y;
            if (!Tropic_resolvePlacementPosition(engine_id, scene->id, &placement, pivot)) continue;

            glm_vec3_copy(pivot, marker_end);
            marker_end[1] += 2.0f;
            _Renderer_pushDebugLine(vertices, &vertex_count, MAX_DEBUG_VERTICES, pivot, marker_end, 1.0f, 0.85f, 0.15f);
        }
    }

    if (vertex_count == 0) return;

    glUseProgram(_renderer_debug_line_program);
    glUniformMatrix4fv(glGetUniformLocation(_renderer_debug_line_program, "view"), 1, GL_FALSE, (const float*)view);
    glUniformMatrix4fv(glGetUniformLocation(_renderer_debug_line_program, "projection"), 1, GL_FALSE, (const float*)projection);
    glBindVertexArray(_renderer_debug_line_vao);
    glBindBuffer(GL_ARRAY_BUFFER, _renderer_debug_line_vbo);
    glBufferData(GL_ARRAY_BUFFER, vertex_count * sizeof(RendererDebugVertex), vertices, GL_DYNAMIC_DRAW);
    glDisable(GL_DEPTH_TEST);
    glDrawArrays(GL_LINES, 0, (GLsizei)vertex_count);
    glEnable(GL_DEPTH_TEST);
}

static void _Renderer_overlayFillRect(int x, int y, int width, int height, float r, float g, float b)
{
    if (width <= 0 || height <= 0) return;

    glScissor(x, y, width, height);
    glClearColor(r, g, b, 1.0f);
    glClear(GL_COLOR_BUFFER_BIT);
}

static void _Renderer_overlayDrawDigit(int x, int y, int scale, int digit, float r, float g, float b)
{
    static const unsigned char digit_segments[10] = {
        0x3F, 0x06, 0x5B, 0x4F, 0x66,
        0x6D, 0x7D, 0x07, 0x7F, 0x6F,
    };
    int thickness = scale;
    int length = scale * 4;
    int inner_height = scale * 5;
    unsigned char mask;

    if (digit < 0 || digit > 9) return;

    mask = digit_segments[digit];

    if ((mask & 0x01u) != 0u) _Renderer_overlayFillRect(x + thickness, y + inner_height * 2 + thickness * 2, length, thickness, r, g, b);
    if ((mask & 0x02u) != 0u) _Renderer_overlayFillRect(x + length + thickness, y + inner_height + thickness, thickness, inner_height, r, g, b);
    if ((mask & 0x04u) != 0u) _Renderer_overlayFillRect(x + length + thickness, y, thickness, inner_height, r, g, b);
    if ((mask & 0x08u) != 0u) _Renderer_overlayFillRect(x + thickness, y - thickness, length, thickness, r, g, b);
    if ((mask & 0x10u) != 0u) _Renderer_overlayFillRect(x, y, thickness, inner_height, r, g, b);
    if ((mask & 0x20u) != 0u) _Renderer_overlayFillRect(x, y + inner_height + thickness, thickness, inner_height, r, g, b);
    if ((mask & 0x40u) != 0u) _Renderer_overlayFillRect(x + thickness, y + inner_height, length, thickness, r, g, b);
}

static void _Renderer_drawFpsOverlay(TropicID engine_id, Tropic *self)
{
    double now;
    int framebuffer_width = 0;
    int framebuffer_height = 0;
    int margin = 12;
    int scale = 4;
    int digit_width = scale * 6;
    int digit_height = scale * 12;
    int spacing = scale * 2;
    int box_width = digit_width * 3 + spacing * 4;
    int box_height = digit_height + spacing * 2;
    int origin_x;
    int origin_y;
    int fps;
    int ten_thousands;
    int thousands;
    int hundreds;
    int tens;
    int ones;

    if (!self || !self->window || !self->fps_overlay_enabled) return;

    now = Tropic_getTime();
    if (!self->fps_overlay_initialized) {
        self->fps_overlay_sample_start_time = now;
        self->fps_overlay_displayed_fps = 0;
        self->fps_overlay_frame_count = 0;
        self->fps_overlay_initialized = true;
    }

    self->fps_overlay_frame_count++;
    if ((now - self->fps_overlay_sample_start_time) >= 0.25) {
        double elapsed = now - self->fps_overlay_sample_start_time;
        if (elapsed > 0.0) {
            self->fps_overlay_displayed_fps = (int)((double)self->fps_overlay_frame_count / elapsed + 0.5);
        }
        self->fps_overlay_frame_count = 0;
        self->fps_overlay_sample_start_time = now;
    }

    glfwGetFramebufferSize(self->window, &framebuffer_width, &framebuffer_height);
    origin_x = margin;
    origin_y = framebuffer_height - margin - box_height + spacing + scale;

    glDisable(GL_DEPTH_TEST);
    glEnable(GL_SCISSOR_TEST);

    _Renderer_overlayFillRect(origin_x,
                              framebuffer_height - margin - box_height,
                              box_width,
                              box_height,
                              0.02f,
                              0.03f,
                              0.06f);

    fps = self->fps_overlay_displayed_fps;
    if (fps < 0) fps = 0;
    //if (fps > 999) fps = 999;

    ten_thousands = (fps / 10000) % 10;
    thousands = (fps / 1000) % 10;
    hundreds = (fps / 100) % 10;
    tens = (fps / 10) % 10;
    ones = fps % 10;

    if ( fps >= 10000) {
        _Renderer_overlayDrawDigit(origin_x, origin_y, scale, ten_thousands, 0.20f, 0.85f, 1.00f);
	}
    if ( fps >= 1000) {
        _Renderer_overlayDrawDigit(origin_x + spacing, origin_y, scale, thousands, 0.20f, 0.85f, 1.00f);
	}
    if (fps >= 100) {
        _Renderer_overlayDrawDigit(origin_x + spacing, origin_y, scale, hundreds, 0.20f, 0.85f, 1.00f);
    }
    if (fps >= 10) {
        _Renderer_overlayDrawDigit(origin_x + spacing * 2 + digit_width, origin_y, scale, tens, 0.20f, 0.85f, 1.00f);
    }
    _Renderer_overlayDrawDigit(origin_x + spacing * 3 + digit_width * 2, origin_y, scale, ones, 0.20f, 0.85f, 1.00f);

    glDisable(GL_SCISSOR_TEST);
    glEnable(GL_DEPTH_TEST);
    (void)engine_id;
}

static void _Renderer_clearFrame(Tropic *self, Scene *scene)
{
    if (!self) return;

    if (scene) {
        glClearColor(scene->background_color[0], scene->background_color[1], scene->background_color[2], 1.0f);
    } else {
        glClearColor(0.1f, 0.1f, 0.1f, 1.0f);
    }

    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
}

static bool _Renderer_prepareCamera(TropicID engine_id,
                                    Tropic *self,
                                    TropicCamera **out_camera,
                                    mat4 view,
                                    mat4 projection)
{
    TropicCamera *camera;
    int width = 1;
    int height = 1;

    if (!self || !out_camera) return false;

    camera = Tropic_getActiveCamera(engine_id);
    if (!camera) return false;

    glfwGetFramebufferSize(self->window, &width, &height);
    if (height == 0) height = 1;

    Tropic_updateCameraFollow(engine_id, camera->id);
    Tropic_applyCameraSpin(engine_id, camera->id);

    glm_lookat(camera->position, camera->target, camera->up, view);
    glm_perspective(glm_rad(camera->fov), (float)width / (float)height, 0.1f, 1000.0f, projection);

    *out_camera = camera;
    return true;
}

static void _Renderer_buildObjectMatrices(const Object *object, mat4 model, mat4 inverse_model)
{
    glm_mat4_identity(model);
    glm_translate(model, object->pos);
    glm_rotate(model, glm_rad(object->rot[0]), (vec3){ 1.0f, 0.0f, 0.0f });
    glm_rotate(model, glm_rad(object->rot[1]), (vec3){ 0.0f, 1.0f, 0.0f });
    glm_rotate(model, glm_rad(object->rot[2]), (vec3){ 0.0f, 0.0f, 1.0f });
    glm_scale(model, object->scale);
    glm_mat4_inv(model, inverse_model);
}

static void _Renderer_drawObject(TropicID engine_id,
                                 Scene *scene,
                                 Object *object,
                                 const TropicCamera *camera,
                                 mat4 view,
                                 mat4 projection)
{
    TropicMaterial *material;
    Mesh *mesh;
    Shader *shader;
    mat4 model;
    mat4 inverse_model;
    GLint model_loc;
    GLint inverse_model_loc;
    GLint view_loc;
    GLint projection_loc;
    GLint camera_pos_loc;
    GLint object_scale_loc;
    GLint time_loc;

    if (!object || !object->active || object->material_id == 0) return;

    material = Tropic_getMaterial(engine_id, object->material_id);
    if (!material || material->mesh_id == 0 || material->shader_id == 0) return;

    mesh = Tropic_getMesh(engine_id, material->mesh_id);
    shader = Tropic_getShader(engine_id, material->shader_id);
    if (!mesh || !shader || mesh->vao == 0 || mesh->ebo_size == 0 || shader->program == 0) return;

    shader_use(shader);
    _Renderer_buildObjectMatrices(object, model, inverse_model);

    model_loc = shader_get_uniform_location(shader, "model");
    inverse_model_loc = shader_get_uniform_location(shader, "inverseModel");
    view_loc = shader_get_uniform_location(shader, "view");
    projection_loc = shader_get_uniform_location(shader, "projection");
    camera_pos_loc = shader_get_uniform_location(shader, "cameraPos");
    object_scale_loc = shader_get_uniform_location(shader, "objectScale");
    time_loc = shader_get_uniform_location(shader, "time");

    if (model_loc >= 0) glUniformMatrix4fv(model_loc, 1, GL_FALSE, (const float*)model);
    if (inverse_model_loc >= 0) glUniformMatrix4fv(inverse_model_loc, 1, GL_FALSE, (const float*)inverse_model);
    if (view_loc >= 0) glUniformMatrix4fv(view_loc, 1, GL_FALSE, (const float*)view);
    if (projection_loc >= 0) glUniformMatrix4fv(projection_loc, 1, GL_FALSE, (const float*)projection);
    if (camera_pos_loc >= 0) glUniform3fv(camera_pos_loc, 1, camera->position);
    if (object_scale_loc >= 0) glUniform3fv(object_scale_loc, 1, object->scale);
    if (time_loc >= 0) glUniform1f(time_loc, (float)Tropic_getTime());

    if (material->render_callback) {
        material->render_callback(engine_id, scene, object, material, material->shader_id, camera);
    }

    glBindVertexArray(mesh->vao);
    glDrawElements(GL_TRIANGLES, (GLsizei)(mesh->ebo_size / sizeof(GLuint)), GL_UNSIGNED_INT, 0);
}

void Tropic_Render( TropicID engine_id )
{
    Tropic *self = Tropic_getById(engine_id);
    Scene *scene;
    TropicCamera *camera;
    mat4 view;
    mat4 projection;

    if (!self) return;

    scene = Tropic_getCurrentScenePtr(self);
    _Renderer_clearFrame(self, scene);

    if (!scene) {
        glfwSwapBuffers(self->window);
        return;
    }

    if (!_Renderer_prepareCamera(engine_id, self, &camera, view, projection)) {
        glfwSwapBuffers(self->window);
        return;
    }

    for (size_t i = 0; i < vector_size(scene->entities); ++i) {
        Object *object = Tropic_getObject(engine_id, scene->entities[i]);
        _Renderer_drawObject(engine_id, scene, object, camera, view, projection);
    }

    _Renderer_drawBeatGridDebug(engine_id, scene, view, projection);

    _Renderer_drawFpsOverlay(engine_id, self);

    glBindVertexArray(0);
    glUseProgram(0);
    glfwSwapBuffers(self->window);
}
