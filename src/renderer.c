#include "tropic.h"
#include "renderer.h"

#include <cglm/cglm.h>
#include <vector.h>

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

    glBindVertexArray(0);
    glUseProgram(0);
    glfwSwapBuffers(self->window);
}
