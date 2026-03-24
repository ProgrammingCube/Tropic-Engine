#include "tropic.h"

#include <math.h>
#include <string.h>

static void _Tropic_getColliderBounds(const Object *object, vec3 out_min, vec3 out_max)
{
    vec3 center;
    vec3 extents;

    if (!object || !out_min || !out_max) return;

    glm_vec3_add(object->pos, object->collider.offset, center);
    extents[0] = fabsf(object->collider.half_extents[0]);
    extents[1] = fabsf(object->collider.half_extents[1]);
    extents[2] = fabsf(object->collider.half_extents[2]);

    out_min[0] = center[0] - extents[0];
    out_min[1] = center[1] - extents[1];
    out_min[2] = center[2] - extents[2];
    out_max[0] = center[0] + extents[0];
    out_max[1] = center[1] + extents[1];
    out_max[2] = center[2] + extents[2];
}

static bool _Tropic_contactListContains(vector( ObjectID ) contacts, ObjectID id)
{
    if (!contacts || id == 0) return false;

    for (size_t i = 0; i < vector_size(contacts); ++i) {
        if (contacts[i] == id) {
            return true;
        }
    }

    return false;
}

static void _Tropic_contactListAddUnique(vector( ObjectID ) *contacts, ObjectID id)
{
    if (!contacts || id == 0) return;
    if (_Tropic_contactListContains(*contacts, id)) return;
    vector_push_back(*contacts, id);
}

static void _Tropic_prepareObjectCollisionState(Object *object)
{
    if (!object) return;

    if (object->previous_collision_ids) {
        vector_free(object->previous_collision_ids);
        object->previous_collision_ids = NULL;
    }

    object->previous_collision_ids = object->current_collision_ids;
    object->current_collision_ids = NULL;
}

static void _Tropic_getBodyVelocity(const Object *object, vec3 out_velocity)
{
    if (!out_velocity) return;

    if (!object || !object->body.enabled) {
        glm_vec3_zero(out_velocity);
        return;
    }

    glm_vec3_copy(object->body.velocity, out_velocity);
}

static void _Tropic_estimateContactNormal(const Object *self, const Object *other, vec3 out_normal)
{
    vec3 self_min;
    vec3 self_max;
    vec3 other_min;
    vec3 other_max;
    vec3 self_center;
    vec3 other_center;
    float overlap[3];
    int axis = 0;

    if (!out_normal) return;
    glm_vec3_zero(out_normal);
    if (!self || !other) return;

    _Tropic_getColliderBounds(self, self_min, self_max);
    _Tropic_getColliderBounds(other, other_min, other_max);
    glm_vec3_add(self->pos, self->collider.offset, self_center);
    glm_vec3_add(other->pos, other->collider.offset, other_center);

    overlap[0] = fminf(self_max[0], other_max[0]) - fmaxf(self_min[0], other_min[0]);
    overlap[1] = fminf(self_max[1], other_max[1]) - fmaxf(self_min[1], other_min[1]);
    overlap[2] = fminf(self_max[2], other_max[2]) - fmaxf(self_min[2], other_min[2]);

    if (overlap[1] < overlap[axis]) axis = 1;
    if (overlap[2] < overlap[axis]) axis = 2;

    out_normal[axis] = self_center[axis] >= other_center[axis] ? 1.0f : -1.0f;
}

static void _Tropic_dispatchCollisionCallback(TropicID engine_id,
                                              Object *self,
                                              Object *other,
                                              TropicCollisionPhase phase,
                                              vec3 normal,
                                              vec3 self_velocity,
                                              vec3 other_velocity)
{
    TropicCollisionEvent event = {0};

    if (!self || !other || !self->collision_callback) return;

    event.self_id = self->id;
    event.other_id = other->id;
    event.phase = phase;
    event.self_flags = self->collider.flags;
    event.other_flags = other->collider.flags;
    event.is_trigger = ((self->collider.flags | other->collider.flags) & TROPIC_COLLIDER_FLAG_TRIGGER) != 0u;
    event.is_solid_contact = ((self->collider.flags & TROPIC_COLLIDER_FLAG_SOLID) != 0u) &&
                             ((other->collider.flags & TROPIC_COLLIDER_FLAG_SOLID) != 0u);
    glm_vec3_copy(normal, event.normal);
    glm_vec3_sub(self_velocity, other_velocity, event.relative_velocity);
    event.impact_speed = fabsf(glm_vec3_dot(event.relative_velocity, event.normal));

    self->collision_callback(engine_id, &event, self->collision_user_data);
}

static bool _Tropic_recordCollisionPair(TropicID engine_id,
                                        Object *first,
                                        Object *second,
                                        vec3 normal_for_first,
                                        vec3 first_velocity,
                                        vec3 second_velocity)
{
    bool already_recorded;
    bool first_had_previous;
    bool second_had_previous;
    vec3 normal_for_second;

    if (!first || !second) return false;

    already_recorded = _Tropic_contactListContains(first->current_collision_ids, second->id);
    _Tropic_contactListAddUnique(&first->current_collision_ids, second->id);
    _Tropic_contactListAddUnique(&second->current_collision_ids, first->id);
    if (already_recorded) return false;

    first_had_previous = _Tropic_contactListContains(first->previous_collision_ids, second->id);
    second_had_previous = _Tropic_contactListContains(second->previous_collision_ids, first->id);
    glm_vec3_negate_to(normal_for_first, normal_for_second);

    _Tropic_dispatchCollisionCallback(engine_id,
                                      first,
                                      second,
                                      first_had_previous ? TROPIC_COLLISION_STAY : TROPIC_COLLISION_ENTER,
                                      normal_for_first,
                                      first_velocity,
                                      second_velocity);
    _Tropic_dispatchCollisionCallback(engine_id,
                                      second,
                                      first,
                                      second_had_previous ? TROPIC_COLLISION_STAY : TROPIC_COLLISION_ENTER,
                                      normal_for_second,
                                      second_velocity,
                                      first_velocity);
    return true;
}

static void _Tropic_dispatchCollisionExits(TropicID engine_id, Scene *scene)
{
    if (!scene) return;

    for (size_t i = 0; i < vector_size(scene->entities); ++i) {
        Object *object = Tropic_getObject(engine_id, scene->entities[i]);

        if (!object || !object->previous_collision_ids) continue;

        for (size_t j = 0; j < vector_size(object->previous_collision_ids); ++j) {
            ObjectID other_id = object->previous_collision_ids[j];
            Object *other;
            vec3 normal;
            vec3 object_velocity;
            vec3 other_velocity;

            if (_Tropic_contactListContains(object->current_collision_ids, other_id)) continue;

            other = Tropic_getObject(engine_id, other_id);
            if (!other || !other->active) continue;

            _Tropic_getBodyVelocity(object, object_velocity);
            _Tropic_getBodyVelocity(other, other_velocity);
            _Tropic_estimateContactNormal(object, other, normal);
            _Tropic_dispatchCollisionCallback(engine_id,
                                              object,
                                              other,
                                              TROPIC_COLLISION_EXIT,
                                              normal,
                                              object_velocity,
                                              other_velocity);
        }
    }
}

static void _Tropic_copyDefaultGravity(vec3 out_gravity)
{
    glm_vec3_copy((vec3){ 0.0f, -18.0f, 0.0f }, out_gravity);
}

static void _Tropic_copyDefaultUp(vec3 out_up)
{
    glm_vec3_copy((vec3){ 0.0f, 1.0f, 0.0f }, out_up);
}

static void _Tropic_getSceneUp(Scene *scene, vec3 out_up)
{
    if (!out_up) return;

    if (!scene || glm_vec3_norm2(scene->world_up[1]) <= 0.000001f) {
        _Tropic_copyDefaultUp(out_up);
        return;
    }

    glm_vec3_copy(scene->world_up[1], out_up);
    glm_vec3_normalize(out_up);
}

static void _Tropic_getNormalizedGravity(Scene *scene, vec3 out_gravity)
{
    if (!out_gravity) return;

    if (!scene) {
        _Tropic_copyDefaultGravity(out_gravity);
        glm_vec3_normalize(out_gravity);
        return;
    }

    if (glm_vec3_norm2(scene->gravity) <= 0.000001f) {
        glm_vec3_zero(out_gravity);
        return;
    }

    glm_vec3_copy(scene->gravity, out_gravity);
    glm_vec3_normalize(out_gravity);
}

static void _Tropic_pickFallbackForward(const vec3 up, vec3 out_forward)
{
    vec3 fallback = { 0.0f, 0.0f, -1.0f };
    if (fabsf(glm_vec3_dot(up, fallback)) > 0.95f) {
        fallback[0] = 1.0f;
        fallback[1] = 0.0f;
        fallback[2] = 0.0f;
    }
    glm_vec3_copy(fallback, out_forward);
}

static void _Tropic_getControlBasisFromGravity(Scene *scene,
                                               vec3 reference_forward,
                                               vec3 out_right,
                                               vec3 out_up,
                                               vec3 out_forward)
{
    vec3 gravity;
    vec3 forward_ref;

    _Tropic_getNormalizedGravity(scene, gravity);
    if (glm_vec3_norm2(gravity) <= 0.000001f) {
        _Tropic_getSceneUp(scene, out_up);
    } else {
        glm_vec3_negate_to(gravity, out_up);
    }

    if (reference_forward && glm_vec3_norm2(reference_forward) > 0.000001f) {
        glm_vec3_normalize_to(reference_forward, forward_ref);
    } else {
        _Tropic_pickFallbackForward(out_up, forward_ref);
    }

    glm_vec3_cross(forward_ref, out_up, out_right);
    if (glm_vec3_norm2(out_right) <= 0.000001f) {
        _Tropic_pickFallbackForward(out_up, forward_ref);
        glm_vec3_cross(forward_ref, out_up, out_right);
    }

    glm_vec3_normalize(out_right);
    glm_vec3_cross(out_up, out_right, out_forward);
    glm_vec3_normalize(out_forward);
}

static bool _Tropic_boundsOverlap(vec3 a_min, vec3 a_max, vec3 b_min, vec3 b_max)
{
    return a_min[0] < b_max[0] && a_max[0] > b_min[0] &&
           a_min[1] < b_max[1] && a_max[1] > b_min[1] &&
           a_min[2] < b_max[2] && a_max[2] > b_min[2];
}

static bool _Tropic_objectsOverlap(const Object *a, const Object *b)
{
    vec3 a_min;
    vec3 a_max;
    vec3 b_min;
    vec3 b_max;

    if (!a || !b || !a->collider.enabled || !b->collider.enabled) return false;

    _Tropic_getColliderBounds(a, a_min, a_max);
    _Tropic_getColliderBounds(b, b_min, b_max);
    return _Tropic_boundsOverlap(a_min, a_max, b_min, b_max);
}

bool Tropic_setSceneGravity(TropicID engine_id, vec3 gravity)
{
    Tropic *self = Tropic_getById(engine_id);
    Scene *scene = Tropic_getCurrentScenePtr(self);
    if (!self || !scene || !gravity) return false;
    glm_vec3_copy(gravity, scene->gravity);
    return true;
}

void Tropic_getSceneGravity(TropicID engine_id, vec3 out_gravity)
{
    Tropic *self = Tropic_getById(engine_id);
    Scene *scene = Tropic_getCurrentScenePtr(self);
    if (!out_gravity) return;
    if (!self || !scene) {
        _Tropic_copyDefaultGravity(out_gravity);
        return;
    }
    glm_vec3_copy(scene->gravity, out_gravity);
}

bool Tropic_buildControlBasis(TropicID engine_id,
                              vec3 reference_forward,
                              vec3 out_right,
                              vec3 out_up,
                              vec3 out_forward)
{
    Tropic *self = Tropic_getById(engine_id);
    Scene *scene = Tropic_getCurrentScenePtr(self);
    if (!self || !scene || !out_right || !out_up || !out_forward) return false;
    _Tropic_getControlBasisFromGravity(scene, reference_forward, out_right, out_up, out_forward);
    return true;
}

bool Tropic_configureObjectCollider(TropicID engine_id,
                                    ObjectID object_id,
                                    bool enabled,
                                    vec3 half_extents,
                                    vec3 offset,
                                    uint32_t flags)
{
    Object *object = Tropic_getObject(engine_id, object_id);
    if (!object || !half_extents || !offset) return false;

    object->collider.enabled = enabled;
    object->collider.type = enabled ? TROPIC_COLLIDER_AABB : TROPIC_COLLIDER_NONE;
    object->collider.flags = flags;
    object->collider.half_extents[0] = fabsf(half_extents[0]);
    object->collider.half_extents[1] = fabsf(half_extents[1]);
    object->collider.half_extents[2] = fabsf(half_extents[2]);
    glm_vec3_copy(offset, object->collider.offset);
    return true;
}

bool Tropic_configurePhysicsBody(TropicID engine_id,
                                 ObjectID object_id,
                                 bool enabled,
                                 bool is_static)
{
    Object *object = Tropic_getObject(engine_id, object_id);
    if (!object) return false;

    object->body.enabled = enabled;
    object->body.is_static = is_static;
    object->body.is_grounded = false;
    object->body.support_object_id = 0;
    object->body.last_contact_object_id = 0;
    if (!enabled) {
        glm_vec3_zero(object->body.velocity);
    }
    if (object->body.ground_friction <= 0.0f) object->body.ground_friction = 18.0f;
    if (object->body.air_friction <= 0.0f) object->body.air_friction = 4.0f;
    return true;
}

int Tropic_stepPhysics(TropicID engine_id, float delta_time)
{
    Tropic *self = Tropic_getById(engine_id);
    Scene *scene = Tropic_getCurrentScenePtr(self);
    vec3 gravity;
    vec3 gravity_dir;
    vec3 up;
    int contacts = 0;

    if (!self || !scene || delta_time <= 0.0f) return 0;

    for (size_t i = 0; i < vector_size(scene->entities); ++i) {
        Object *object = Tropic_getObject(engine_id, scene->entities[i]);
        if (!object) continue;
        _Tropic_prepareObjectCollisionState(object);
    }

    Tropic_getSceneGravity(engine_id, gravity);
    if (glm_vec3_norm2(gravity) <= 0.000001f) {
        glm_vec3_zero(gravity_dir);
        _Tropic_getSceneUp(scene, up);
    } else {
        glm_vec3_normalize_to(gravity, gravity_dir);
        glm_vec3_negate_to(gravity_dir, up);
    }

    for (size_t i = 0; i < vector_size(scene->entities); i++) {
        Object *object = Tropic_getObject(engine_id, scene->entities[i]);
        vec3 sweep_velocity;
        if (!object || !object->active || !object->body.enabled || object->body.is_static) continue;

        object->body.is_grounded = false;
        object->body.support_object_id = 0;
        object->body.last_contact_object_id = 0;

        object->body.velocity[0] += gravity[0] * delta_time;
        object->body.velocity[1] += gravity[1] * delta_time;
        object->body.velocity[2] += gravity[2] * delta_time;
        glm_vec3_copy(object->body.velocity, sweep_velocity);

        for (int axis = 0; axis < 3; axis++) {
            float axis_delta = object->body.velocity[axis] * delta_time;
            if (fabsf(axis_delta) <= 0.000001f) continue;

            object->pos[axis] += axis_delta;

            for (size_t j = 0; j < vector_size(scene->entities); j++) {
                Object *other = Tropic_getObject(engine_id, scene->entities[j]);
                vec3 self_min;
                vec3 self_max;
                vec3 other_min;
                vec3 other_max;
                vec3 normal = { 0.0f, 0.0f, 0.0f };
                vec3 other_velocity;
                float correction;
                bool pair_owner;

                if (!other || other == object || !other->active || !other->collider.enabled) continue;
                if (!_Tropic_objectsOverlap(object, other)) continue;

                object->body.last_contact_object_id = other->id;
                _Tropic_getBodyVelocity(other, other_velocity);
                pair_owner = !other->body.enabled || other->body.is_static || object->id < other->id;

                if ((other->collider.flags & TROPIC_COLLIDER_FLAG_SOLID) == 0u) continue;

                _Tropic_getColliderBounds(object, self_min, self_max);
                _Tropic_getColliderBounds(other, other_min, other_max);

                if (axis_delta > 0.0f) {
                    correction = other_min[axis] - self_max[axis];
                    normal[axis] = -1.0f;
                } else {
                    correction = other_max[axis] - self_min[axis];
                    normal[axis] = 1.0f;
                }

                if (pair_owner && _Tropic_recordCollisionPair(engine_id,
                                                              object,
                                                              other,
                                                              normal,
                                                              sweep_velocity,
                                                              other_velocity)) {
                    contacts++;
                }

                object->pos[axis] += correction;
                object->body.velocity[axis] = 0.0f;

                if (glm_vec3_dot(normal, up) > 0.5f) {
                    object->body.is_grounded = true;
                    object->body.support_object_id = other->id;
                }
            }
        }

        for (size_t j = 0; j < vector_size(scene->entities); j++) {
            Object *other = Tropic_getObject(engine_id, scene->entities[j]);
            vec3 normal;
            vec3 other_velocity;
            bool pair_owner;
            if (!other || other == object || !other->active || !other->collider.enabled) continue;
            if (_Tropic_objectsOverlap(object, other)) {
                object->body.last_contact_object_id = other->id;
                pair_owner = !other->body.enabled || other->body.is_static || object->id < other->id;
                if (!pair_owner) continue;

                _Tropic_estimateContactNormal(object, other, normal);
                _Tropic_getBodyVelocity(other, other_velocity);
                if (_Tropic_recordCollisionPair(engine_id,
                                               object,
                                               other,
                                               normal,
                                               sweep_velocity,
                                               other_velocity)) {
                    contacts++;
                }
            }
        }
    }

    _Tropic_dispatchCollisionExits(engine_id, scene);

    return contacts;
}
