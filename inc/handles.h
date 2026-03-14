#ifndef HANDLES_H
#define HANDLES_H

#include "id_manager.h"
#include <stdint.h>

#ifndef GLFW_INCLUDE_NONE
#define GLFW_INCLUDE_NONE
#endif

#include <GLFW/glfw3.h>

/* Typed aliases for clarity */
typedef Handle TropicID;
typedef Handle SceneID;
typedef Handle64 ObjectID;
typedef Handle64 MeshID;
typedef Handle64 TextureID;
typedef Handle64 CameraID;
typedef Handle64 ShaderID;
typedef Handle64 MaterialID;

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

static inline MeshID Tropic_makeMeshID(SceneID scene_id, Handle local_id)
{
	return (MeshID)Tropic_packScopedID(scene_id, local_id);
}

static inline TextureID Tropic_makeTextureID(SceneID scene_id, Handle local_id)
{
	return (TextureID)Tropic_packScopedID(scene_id, local_id);
}

static inline ShaderID Tropic_makeShaderID(SceneID scene_id, Handle local_id)
{
	return (ShaderID)Tropic_packScopedID(scene_id, local_id);
}

static inline MaterialID Tropic_makeMaterialID(SceneID scene_id, Handle local_id)
{
	return (MaterialID)Tropic_packScopedID(scene_id, local_id);
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

static inline SceneID Tropic_getSceneIDFromMeshID(MeshID id)
{
	return Tropic_unpackScopedSceneID((uint64_t)id);
}

static inline Handle Tropic_getLocalHandleFromMeshID(MeshID id)
{
	return Tropic_unpackScopedLocalID((uint64_t)id);
}

static inline SceneID Tropic_getSceneIDFromTextureID(TextureID id)
{
	return Tropic_unpackScopedSceneID((uint64_t)id);
}

static inline Handle Tropic_getLocalHandleFromTextureID(TextureID id)
{
	return Tropic_unpackScopedLocalID((uint64_t)id);
}

static inline SceneID Tropic_getSceneIDFromShaderID(ShaderID id)
{
	return Tropic_unpackScopedSceneID((uint64_t)id);
}

static inline Handle Tropic_getLocalHandleFromShaderID(ShaderID id)
{
	return Tropic_unpackScopedLocalID((uint64_t)id);
}

static inline SceneID Tropic_getSceneIDFromMaterialID(MaterialID id)
{
	return Tropic_unpackScopedSceneID((uint64_t)id);
}

static inline Handle Tropic_getLocalHandleFromMaterialID(MaterialID id)
{
	return Tropic_unpackScopedLocalID((uint64_t)id);
}

/* GLFWwindow "handle" */
typedef GLFWwindow TropicWindowID;

#endif /* HANDLES_H */
