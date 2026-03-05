#ifndef HANDLES_H
#define HANDLES_H

#include "id_manager.h"
#include <stdint.h>
#include <GLFW/glfw3.h>

/* Typed aliases for clarity */
typedef Handle TropicID;
typedef Handle SceneID;
typedef Handle64 ObjectID;
typedef Handle MeshID;
typedef Handle TextureID;
typedef Handle64 CameraID;
typedef Handle ShaderID;

/* Scoped ID layout: [scene:32][local:32] */
static inline uint64_t Tropic_packScopedID(SceneID scene_id, Handle local_id)
{
	return ((uint64_t)scene_id << 32) | (uint64_t)local_id;
}

static inline SceneID Tropic_unpackScopedSceneID(uint64_t scoped_id)
{
	return (SceneID)(scoped_id >> 32);
}

static inline Handle Tropic_unpackScopedLocalID(uint64_t scoped_id)
{
	return (Handle)(scoped_id & 0xFFFFFFFFu);
}

static inline ObjectID Tropic_makeObjectID(SceneID scene_id, Handle local_id)
{
	return (ObjectID)Tropic_packScopedID(scene_id, local_id);
}

static inline CameraID Tropic_makeCameraID(SceneID scene_id, Handle local_id)
{
	return (CameraID)Tropic_packScopedID(scene_id, local_id);
}

static inline SceneID Tropic_getSceneIDFromObjectID(ObjectID id)
{
	return Tropic_unpackScopedSceneID((uint64_t)id);
}

static inline Handle Tropic_getLocalHandleFromObjectID(ObjectID id)
{
	return Tropic_unpackScopedLocalID((uint64_t)id);
}

static inline SceneID Tropic_getSceneIDFromCameraID(CameraID id)
{
	return Tropic_unpackScopedSceneID((uint64_t)id);
}

static inline Handle Tropic_getLocalHandleFromCameraID(CameraID id)
{
	return Tropic_unpackScopedLocalID((uint64_t)id);
}

/* GLFWwindow "handle" */
typedef GLFWwindow TropicWindowID;

#endif /* HANDLES_H */
