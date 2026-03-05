# 🌴 Tropic Game Engine 🏝️

Imagine, if you would, a tropical paradise. Just you, a laptop, and the warm waves lapping at your feet. Nothing else in the world matters. It is you and your code, and the simple environment you are in.

Tropic is like this, a simple environment yet you want for nothing. It takes care of the deep backend while you are free to do whatever you want - you ARE on vacation, after all!

This bare bones game engine boasts simple scene management, multiple cameras, world/scene/camera/gravity rotation. More features to come!

---

## 📖 Table of Contents

- [Requirements](#requirements)
- [Build & Run](#build-and-run)
- [How It Works (from `engine_test`)](#how-it-works)
- [Core Data Model](#core-data-model)
- [Handle Model (Updated)](#handle-model-updated)
- [Public API Snapshot](#public-api-snapshot)
- [Render Workflow](#render-workflow)
- [Level Loading Notes](#level-loading-notes)
- [Project Layout](#project-layout)
- [Wiki Starter](#wiki-starter)
- [Docs Maintenance](#docs-maintenance)

---

<a id="requirements"></a>
## 🛠️ Requirements

Tropic currently builds as a C11 static library with a test executable via CMake.

### Dependencies

- [cJSON](https://github.com/DaveGamble/cJSON)
- [CVector](https://github.com/ProgrammingCube/c_vector)
- [GLFW 3](https://www.glfw.org/)
- OpenGL
- [cglm](https://github.com/recp/cglm) (fetched by CMake)

### Tooling

- CMake 3.14+
- A C compiler with C11 support (`gcc`/`clang`)

---

<a id="build-and-run"></a>
## 🚀 Build & Run

```bash
cmake -S . -B build
cmake --build build
./build/tests/tropic_test
```

> If your distro uses different package names for dependencies, install equivalents for `cjson`, `glfw3`, and OpenGL dev headers.

---

<a id="how-it-works"></a>
## 🌊 How It Works (from `engine_test`)

The current test app (`tests/engine_test.c`) demonstrates the full expected loop:

1. Create engine: `Tropic_create`
2. Set active engine: `Tropic_setActiveEngine`
3. Create window/context: `Tropic_CreateWindow`
4. Parse and convert level JSON: `parseLevel` + `levelspec_to_objects`
5. Load objects into current scene: `Tropic_loadObjects`
6. Per-frame loop:
   - `Tropic_Update`
   - input-driven object movement (`Tropic_translateObject`)
   - `Tropic_Render`
7. Shutdown: `Tropic_destroy`

This is the best “source of truth” right now for how all modules fit together.

---

<a id="core-data-model"></a>
## 🥥 Core Data Model

### `Tropic`

Root engine context with:
- GLFW window pointer
- global `TropicGameState`
- active scene handle
- scene handle list + scene ID manager

### `Scene`

Owns scene-local pools and runtime data:
- object/camera/mesh/texture/shader managers
- entity list and camera list
- active camera
- default platform mesh/shader handles for renderer bootstrapping

### `Object`

Per-entity state:
- `ObjectID`
- transform (`pos`, `scale`, `rot`)
- object type (`ObjectType`)
- mesh + shader handles
- active flag

### `TropicGameState`

Global gameplay metadata:
- `game_title`
- `level_name`
- `play_speed`

---

<a id="handle-model-updated"></a>
## 🧩 Handle Model (Updated)

`ObjectID` and `CameraID` are scoped handles that include their owning scene.

- Layout: `[scene:32][local:32]`
- High 32 bits: `SceneID`
- Low 32 bits: scene-local pool handle

Helper functions in `inc/handles.h`:

- `Tropic_makeObjectID`, `Tropic_makeCameraID`
- `Tropic_getSceneIDFromObjectID`, `Tropic_getLocalHandleFromObjectID`
- `Tropic_getSceneIDFromCameraID`, `Tropic_getLocalHandleFromCameraID`

Practical behavior:

- `Tropic_getObject` and `Tropic_getCamera` resolve scene from the ID itself.
- You can query scene ownership directly without scanning all scenes.

---

<a id="public-api-snapshot"></a>
## 🔧 Public API Snapshot

> API list here is aligned to current headers (`inc/tropic.h`, `inc/object.h`, `inc/scene.h`, `inc/camera.h`).

### Engine / lifecycle

- `Tropic_create`, `Tropic_destroy`, `Tropic_getById`, `Tropic_getByPtr`
- `Tropic_CreateWindow`, `Tropic_getWindow`, `Tropic_Update`, `Tropic_Render`
- `Tropic_getGameState`
- `Tropic_loadObjects`, `Tropic_getNumObjectsInScene`, `Tropic_getNumObjectsByType`
- `Tropic_setActiveEngine`, `Tropic_getActiveEnginePtr`, `Tropic_getActiveEngineId`

### Scene APIs

- `Tropic_createScene`, `Tropic_getSceneByID`, `Tropic_getCurrentScene`
- `Tropic_getCurrentSceneID`, `Tropic_setCurrentScene`, `Tropic_freeScene`

### Object APIs

- `Tropic_newObject`, `Tropic_getObject`, `Tropic_freeObject`
- `Tropic_getObjectScene`, `Tropic_getObjectEngine`
- `Tropic_findFirstObjectOfType`
- `Tropic_translateObject`, `Tropic_rotateObject`, `Tropic_scaleObject`

### Camera APIs

- `Tropic_newCamera`, `Tropic_setCamera`, `Tropic_getCamera`
- `Tropic_getActiveCamera`, `Tropic_getActiveCameraId`
- `Tropic_lookAtObjectById`
- `Tropic_getCameraScene`
- camera property getters/setters (`FOV`, `position`, `target`, `up`, `roll`)

### Resource pool APIs

- Mesh: `Tropic_newMesh`, `Tropic_getMesh`, `Tropic_freeMesh`
- Texture: `Tropic_newTexture`, `Tropic_getTexture`, `Tropic_freeTexture`
- Shader: `Tropic_newShader`, `Tropic_getShader`, `Tropic_freeShader`

---

<a id="render-workflow"></a>
## 🎬 Render Workflow

Use one of these expected workflows.

### A) Level-driven rendering (recommended)

1. `Tropic_create`
2. `Tropic_setActiveEngine`
3. `Tropic_CreateWindow`
4. `parseLevel` and `levelspec_to_objects` (test layer)
5. `Tropic_loadObjects`
6. Main loop: `Tropic_Update` -> optional transform edits (`Tropic_translateObject`) -> `Tropic_Render`
7. `Tropic_destroy`

This is what `tests/engine_test.c` follows.

### B) Manual object rendering (no level file)

1. Create engine/window: `Tropic_create`, `Tropic_setActiveEngine`, `Tropic_CreateWindow`
2. Ensure active scene exists or switch scene: `Tropic_createScene`, `Tropic_setCurrentScene`
3. Create and activate camera: `Tropic_newCamera`, `Tropic_setCamera`
4. Spawn object: `Tropic_newObject`
5. Make object renderable:
- For `TYPE_PLATFORM`, leave `mesh_id` and `shader_id` as `0` to use default scene platform assets
- For `TYPE_GENERIC`, assign valid mesh/shader IDs from `Tropic_newMesh` and `Tropic_newShader`
6. In your loop: `Tropic_Update` and `Tropic_Render`

Minimal frame loop:

```c
while (Tropic_Update(engine_id)) {
  Tropic_Render(engine_id);
}
```

Troubleshooting when only clear color appears:

- Level file path failed, so no objects were loaded
- No active camera in the current scene
- Object is `TYPE_GENERIC` but missing mesh/shader handles
- Scene changed and IDs used belong to a different scene

---

<a id="level-loading-notes"></a>
## 🧭 Level Loading Notes

- The runtime engine symbol `Tropic_parseLevel` is currently a weak stub in `src/tropic.c`.
- Actual JSON parsing used by the test app lives in `tests/level_loader.c` via:
  - `parseLevel(const char* path, int* out_num_objects)`
  - `levelspec_to_objects(LevelSpec* spec, TropicID engine, int* out_num_objects)`
- Current object families in level files: `platforms`, `spikes`, `jumppads`.

Sample level file: `assets/levels/test_level.json`

---

<a id="project-layout"></a>
## 🗂️ Project Layout

- `inc/` — public headers and core engine types
- `src/` — engine implementation
- `tests/` — test executable + level parsing helper
- `assets/levels/` — test level JSON
- `assets/shaders/` — default platform shaders

---

<a id="wiki-starter"></a>
## 📚 Wiki Starter

To keep this README focused, long-form docs are started under `docs/wiki/`:

- [docs/wiki/Home.md](docs/wiki/Home.md)
- [docs/wiki/Engine-API.md](docs/wiki/Engine-API.md)
- [docs/wiki/Level-Format.md](docs/wiki/Level-Format.md)

If you later enable GitHub Wiki for this repo, these files can be copied over as initial pages.

---

<a id="docs-maintenance"></a>
## 🧰 Docs Maintenance

Use this lightweight rule of thumb to keep docs accurate without extra overhead:

1. **When a public function signature changes**
  - Update `inc/*.h`
  - Update [docs/wiki/Engine-API.md](docs/wiki/Engine-API.md)
  - If the change affects first-time users, update [README.md](README.md)

2. **When runtime behavior changes** (scene flow, render setup, object lifecycle)
  - Update [README.md](README.md) in “How It Works” or “Core Data Model”
  - Add deeper notes to wiki pages (preferred for long explanations)

3. **When level JSON format changes**
  - Update parser expectations in [docs/wiki/Level-Format.md](docs/wiki/Level-Format.md)
  - Keep sample asset references current (`assets/levels/test_level.json`)

4. **Before merging doc-impacting changes**
  - Quick check: README links still work
  - Quick check: wiki page links still work
  - Quick check: function names in docs match headers exactly

This keeps the README concise while letting the wiki grow into full reference docs.