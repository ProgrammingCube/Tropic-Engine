# 🌴 Tropic Game Engine 🏝️

Imagine, if you would, a tropical paradise. Just you, a laptop, and the warm waves lapping at your feet. Nothing else in the world matters. It is you and your code, and the simple environment you are in.

Tropic is like this, a simple environment yet you want for nothing. It takes care of the deep backend while you are free to do whatever you want - you ARE on vacation, after all!

This bare bones game engine boasts simple scene management, multiple cameras, world/scene/camera/gravity rotation. More features to come!

---

## 📖 Table of Contents
- [Requirements](#requirements)
- [Core Structures](#core-structures)
  - [Tropic Engine](#struct-tropic)
  - [Scene](#struct-scene)
  - [Object](#struct-object)
  - [TropicGameState](#struct-tropicgamestate)
- [Engine API](#engine-api)
  - [Engine Handle APIs](#engine-handle-apis)
    - [Tropic_create](#tropic_create)
    - [Tropic_getById](#tropic_getbyid)
    - [Tropic_getByPtr](#tropic_getbyptr)
    - [Tropic_destroy](#tropic_destroy)
    - [Tropic_getGameState](#tropic_getgamestate)
  - [Core Lifecycle APIs](#core-lifecycle-apis)
    - [Tropic_parseLevel](#tropic_parselevel)
    - [Tropic_loadObjects](#tropic_loadobjects)
    - [Tropic_getNumObjectsInScene](#tropic_getnumobjectsinscene)
    - [Tropic_getNumObjectsByType](#tropic_getnumobjectsbytype)
    - [Tropic_update](#tropic_update)
    - [Tropic_render](#tropic_render)
    - [Tropic_cleanup](#tropic_cleanup)
  - [Object Pool APIs](#object-pool-apis)
    - [Tropic_newObject](#tropic_newobject)
    - [Tropic_getObject](#tropic_getobject)
    - [Tropic_freeObject](#tropic_freeobject)
  - [Mesh Pool APIs](#mesh-pool-apis)
    - [Tropic_newMesh](#tropic_newmesh)
    - [Tropic_getMesh](#tropic_getmesh)
    - [Tropic_freeMesh](#tropic_freemesh)
  - [Texture Pool APIs](#texture-pool-apis)
    - [Tropic_newTexture](#tropic_newtexture)
    - [Tropic_getTexture](#tropic_gettexture)
    - [Tropic_freeTexture](#tropic_freetexture)

---

## 🛠️ Requirements

### Libraries
<a href="https://github.com/ProgrammingCube/c_vector" target="_blank">CVector</a>  
<a href="https://github.com/DaveGamble/cJSON" target="_blank">cJSON</a>

## 🥥 Core Structures

### Struct: Tropic
*(Add description of the `Tropic` engine struct here. E.g., its role as the root context, how it manages resource ID managers, the current scene, etc.)*

### Struct: Scene
*(Add description of the `Scene` struct here. E.g., how it manages spatial partitions, environments, cameras, and local object hierarchies.)*

### Struct: Object
*(Add description of the `Object` struct here. E.g., what components make up a game object, transform data, physics bodies, and mesh/texture assignments.)*

### Struct: TropicGameState
*(Add description of the `TropicGameState` struct here. E.g., how it tracks current engine state, paused/playing status, physics ticks, or global world data.)*

---

## 🌊 Engine API

### Engine Handle APIs

#### `Tropic_create`
Initializes and allocates a new instance of the Tropic engine.
* **Parameters:** None
* **Returns:** `TropicID` - A unique identifier handle for the newly created engine instance.

#### `Tropic_getById`
Retrieves the engine instance pointer associated with a given handle ID.
* **Parameters:** 
  * `id` (`TropicID`): The handle ID of the target engine.
* **Returns:** `Tropic*` - A pointer to the Tropic engine struct. Returns `NULL` if the ID is invalid.

#### `Tropic_getByPtr`
Retrieves the handle ID for a given engine pointer.
* **Parameters:**
  * `ptr` (`Tropic*`): A pointer to an active Tropic engine instance.
* **Returns:** `TropicID` - The handle ID associated with the provided pointer.

#### `Tropic_destroy`
Destroys an engine instance and cleans up its associated high-level memory allocations.
* **Parameters:**
  * `id` (`TropicID`): The handle ID of the engine to destroy.
* **Returns:** `bool` - `true` if destruction was successful, `false` otherwise.

#### `Tropic_getGameState`
Fetches a pointer to the engine's current GameState struct.
* **Parameters:**
  * `id` (`TropicID`): The handle ID of the target engine.
* **Returns:** `TropicGameState*` - Pointer to the game state structure representing current engine status.

---

### Core Lifecycle APIs

#### `Tropic_parseLevel`
Parses a level file (e.g., JSON) and prepares it for loading.
* **Parameters:**
  * `engine` (`TropicID`): The engine handle ID.
  * `level_path` (`const char*`): The file path to the level asset.
  * `out_num_objects` (`int*`): A pointer to an integer that will be populated with the number of objects parsed from the level.
* **Returns:** `void*` - A pointer to the parsed level data (e.g., `ObjectSpec` array).

#### `Tropic_loadObjects`
Loads an array of object specifications into the engine's active scene and object pools.
* **Parameters:**
  * `engine` (`TropicID`): The engine handle ID.
  * `objects` (`ObjectSpec*`): Pointer to the array of parsed object specifications.
  * `num_objects` (`int`): The number of objects to load from the array.
* **Returns:** Void.

#### `Tropic_getNumObjectsInScene`
Counts the total number of instantiated objects currently residing in the active scene.
* **Parameters:**
  * `engine` (`TropicID`): The engine handle ID.
* **Returns:** `int` - The total count of active objects.

#### `Tropic_getNumObjectsByType`
Counts the number of instantiated objects belonging to a specific `ObjectType`.
* **Parameters:**
  * `engine` (`TropicID`): The engine handle ID.
  * `type` (`ObjectType`): The type classification to filter by.
* **Returns:** `int` - The total count of matching objects.

#### `Tropic_update`
Advances the game logic, physics, and state by one frame based on elapsed time.
* **Parameters:**
  * `self` (`Tropic*`): Pointer to the engine instance.
  * `delta_time` (`float`): The time elapsed (in seconds) since the last frame.
* **Returns:** Void.

#### `Tropic_render`
Dispatches the draw calls for the current scene, drawing all visible meshes/textures via OpenGL.
* **Parameters:**
  * `self` (`Tropic*`): Pointer to the engine instance.
* **Returns:** Void.

#### `Tropic_cleanup`
Performs a deep clean of the engine instance, freeing all resource pools (objects, meshes, textures) and terminating the scene cleanly.
* **Parameters:**
  * `self` (`Tropic*`): Pointer to the engine instance.
* **Returns:** Void.

---

### Object Pool APIs

#### `Tropic_newObject`
Allocates and registers a new game object inside the engine's Object IDManager pool.
* **Parameters:**
  * `self` (`Tropic*`): Pointer to the engine instance.
  * `proto` (`const Object*`): An optional prototype object to clone data from (can be `NULL` for an empty object).
* **Returns:** `ObjectID` - The handle ID for the newly created object.

#### `Tropic_getObject`
Retrieves a pointer to an active object from the pool using its ID.
* **Parameters:**
  * `self` (`Tropic*`): Pointer to the engine instance.
  * `id` (`ObjectID`): The handle ID of the target object.
* **Returns:** `Object*` - Pointer to the object, or `NULL` if invalid/freed.

#### `Tropic_freeObject`
Frees an object from the pool and recycles its ID.
* **Parameters:**
  * `self` (`Tropic*`): Pointer to the engine instance.
  * `id` (`ObjectID`): The handle ID of the object to free.
* **Returns:** `bool` - `true` if successfully freed, `false` otherwise.

---

### Mesh Pool APIs

#### `Tropic_newMesh`
Loads and registers a new Mesh into the engine's Mesh IDManager pool.
* **Parameters:**
  * `self` (`Tropic*`): Pointer to the engine instance.
  * `proto` (`const Mesh*`): Pointer to the mesh data/prototype to copy into the pool.
* **Returns:** `MeshID` - The handle ID for the registered mesh.

#### `Tropic_getMesh`
Retrieves a pointer to a registered mesh.
* **Parameters:**
  * `self` (`Tropic*`): Pointer to the engine instance.
  * `id` (`MeshID`): The handle ID of the target mesh.
* **Returns:** `Mesh*` - Pointer to the mesh struct, or `NULL` if invalid/freed.

#### `Tropic_freeMesh`
Releases mesh data from GPU/CPU memory and frees its ID in the pool.
* **Parameters:**
  * `self` (`Tropic*`): Pointer to the engine instance.
  * `id` (`MeshID`): The handle ID of the mesh to free.
* **Returns:** `bool` - `true` if successfully freed, `false` otherwise.

---

### Texture Pool APIs

#### `Tropic_newTexture`
Loads and registers a new Texture into the engine's Texture IDManager pool.
* **Parameters:**
  * `self` (`Tropic*`): Pointer to the engine instance.
  * `proto` (`const Texture*`): Pointer to the texture data/prototype to copy into the pool.
* **Returns:** `TextureID` - The handle ID for the registered texture.

#### `Tropic_getTexture`
Retrieves a pointer to a registered texture.
* **Parameters:**
  * `self` (`Tropic*`): Pointer to the engine instance.
  * `id` (`TextureID`): The handle ID of the target texture.
* **Returns:** `Texture*` - Pointer to the texture struct, or `NULL` if invalid/freed.

#### `Tropic_freeTexture`
Releases texture data from GPU/CPU memory and frees its ID in the pool.
* **Parameters:**
  * `self` (`Tropic*`): Pointer to the engine instance.
  * `id` (`TextureID`): The handle ID of the texture to free.
* **Returns:** `bool` - `true` if successfully freed, `false` otherwise.