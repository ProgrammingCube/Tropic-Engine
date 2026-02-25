# Engine API

This page tracks the public API currently exposed by headers in `inc/`.

## Engine / Lifecycle

- `Tropic_create`
- `Tropic_getById`
- `Tropic_getByPtr`
- `Tropic_destroy`
- `Tropic_getGameState`
- `Tropic_CreateWindow`
- `Tropic_getWindow`
- `Tropic_Update`
- `Tropic_Render`
- `Tropic_loadObjects`
- `Tropic_getNumObjectsInScene`
- `Tropic_getNumObjectsByType`

## Scene

- `Tropic_createScene`
- `Tropic_getSceneByID`
- `Tropic_getCurrentScene`
- `Tropic_getCurrentSceneID`
- `Tropic_setCurrentScene`
- `Tropic_freeScene`

## Object

- `Tropic_newObject`
- `Tropic_getObject`
- `Tropic_freeObject`
- `Tropic_findFirstObjectOfType`
- `Tropic_translateObject`
- `Tropic_rotateObject`
- `Tropic_scaleObject`

## Camera

- `Tropic_newCamera`
- `Tropic_setCamera`
- `Tropic_getCamera`
- `Tropic_getActiveCamera`
- `Tropic_getActiveCameraId`
- `Tropic_lookAtObjectById`
- `Tropic_setCameraFOV` / `Tropic_getCameraFOV`
- `Tropic_setCameraPosition` / `Tropic_getCameraPosition`
- `Tropic_setCameraTarget` / `Tropic_getCameraTarget`
- `Tropic_setCameraUp` / `Tropic_getCameraUp`
- `Tropic_setCameraRoll` / `Tropic_getCameraRoll`

## Resources

- Mesh: `Tropic_newMesh`, `Tropic_getMesh`, `Tropic_freeMesh`
- Texture: `Tropic_newTexture`, `Tropic_getTexture`, `Tropic_freeTexture`
- Shader: `Tropic_newShader`, `Tropic_getShader`, `Tropic_freeShader`

## Notes

- `Tropic_parseLevel` currently exists as a weak stub in runtime code.
- Test level parsing/conversion is implemented in `tests/level_loader.c`.
