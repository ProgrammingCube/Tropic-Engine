#include "tropic.h"
#include "renderer.h"

#include <cglm/cglm.h>
#include <vector.h>

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

    _Renderer_drawFpsOverlay(engine_id, self);

    glBindVertexArray(0);
    glUseProgram(0);
    glfwSwapBuffers(self->window);
}
