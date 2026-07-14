#ifndef TROPIC_H
#define TROPIC_H

#include <vector.h>
#include <cjson/cJSON.h>
#include <stddef.h>
#include <glad/glad.h>
#include <GLFW/glfw3.h>
#include "miniaudio.h"
#include "tropic_gamestate.h"
#include "camera.h"
#include "material.h"
#include "object.h"
#include "scene.h"
#include "handles.h"
#include "id_manager.h"
#include "mesh.h"
#include "texture.h"
#include "shader.h"
#include "renderer.h"
#include "audio.h"

extern TropicID _TROPIC_ACTIVE_ENGINE;

#define Tropic_getTime() glfwGetTime()

// probably should move at least objects and cameras to scenes
// refactor Scene to be SceneID vector and have Scene*'s be in the memory pool
typedef struct sTropic
{
    GLFWwindow* window;
    /* change to pointer */
    TropicGameState state;
    //Scene* current_scene;
    SceneID current_scene;
    vector( SceneID ) scenes;

    /* Resource pools */
    IDManager* scene_manager;

    double last_update_time;
    bool has_last_update_time;
    bool fps_overlay_enabled;
    double fps_overlay_sample_start_time;
    int fps_overlay_frame_count;
    int fps_overlay_displayed_fps;
    bool fps_overlay_initialized;
    bool beat_grid_debug_enabled;

    ma_engine audio_engine;
    ma_sound music_sound;
    bool audio_initialized;
    bool music_loaded;
    float master_volume;
    //Renderer* renderer;
    // Add more fields as needed
} Tropic;


/* Engine handle APIs */
TropicID Tropic_create(void);
Tropic*  Tropic_getById(TropicID id);
TropicID Tropic_getByPtr(Tropic* ptr);
bool     Tropic_destroy(TropicID id);
TropicGameState* Tropic_getGameState( TropicID id );

TropicWindowID* Tropic_getWindow( TropicID engine_id );

/* Core lifecycle */
TropicWindowID* Tropic_CreateWindow( TropicID engine_id, int width, int height, const char* title, bool fullscreen );
int Tropic_Update( TropicID engine_id );

bool Tropic_setKeyCallback(TropicID engine_id, void* callback);

void* Tropic_parseLevel(TropicID engine, const char* level_path, int* out_num_objects );
void Tropic_loadObjects( TropicID engine, ObjectSpec* objects, int num_objects );
int Tropic_getNumObjectsInScene( TropicID engine );
int Tropic_getNumObjectsByType( TropicID engine, ObjectType type );
bool Tropic_getSceneBeatGridSettings(TropicID engine_id,
                                     SceneID scene_id,
                                     TropicBeatGridSettings *out_settings);
bool Tropic_setSceneBeatGridSettings(TropicID engine_id,
                                     SceneID scene_id,
                                     const TropicBeatGridSettings *settings);
size_t Tropic_getSceneTrackAnchorCount(TropicID engine_id, SceneID scene_id);
bool Tropic_getSceneTrackAnchor(TropicID engine_id,
                                SceneID scene_id,
                                size_t anchor_index,
                                TropicTrackAnchor *out_anchor);
bool Tropic_clearSceneTrackAnchors(TropicID engine_id, SceneID scene_id);
bool Tropic_addSceneTrackAnchor(TropicID engine_id,
                                SceneID scene_id,
                                const TropicTrackAnchor *anchor);
bool Tropic_buildPlacementFrame(TropicID engine_id,
                                SceneID scene_id,
                                const TropicTrackPlacement *placement,
                                TropicTrackFrame *out_frame);
bool Tropic_resolvePlacementPosition(TropicID engine_id,
                                     SceneID scene_id,
                                     const TropicTrackPlacement *placement,
                                     vec3 out_position);
float Tropic_getCurrentSceneMusicBeat(TropicID engine_id);
bool Tropic_getCurrentSceneMusicBeatTime(TropicID engine_id,
                                         TropicBeatTime *out_time,
                                         float *out_exact_beat);
bool Tropic_setSceneGravity( TropicID engine_id, vec3 gravity );
void Tropic_getSceneGravity( TropicID engine_id, vec3 out_gravity );
bool Tropic_buildControlBasis( TropicID engine_id,
                               vec3 reference_forward,
                               vec3 out_right,
                               vec3 out_up,
                               vec3 out_forward );
bool Tropic_configureObjectCollider( TropicID engine_id,
                                     ObjectID object_id,
                                     bool enabled,
                                     vec3 half_extents,
                                     vec3 offset,
                                     uint32_t flags );
bool Tropic_configurePhysicsBody( TropicID engine_id,
                                  ObjectID object_id,
                                  bool enabled,
                                  bool is_static );
bool Tropic_setObjectCollisionCallback( TropicID engine_id,
                                        ObjectID object_id,
                                        TropicCollisionCallback callback,
                                        void *user_data );
int Tropic_stepPhysics( TropicID engine_id, float delta_time );

// Sets active engine by TropicID. Needs to be an engine global
bool Tropic_setActiveEngine( TropicID engine_id );
Tropic* Tropic_getActiveEnginePtr( void );
TropicID Tropic_getActiveEngineId( void );

/* Mesh pool APIs */
MeshID   Tropic_newMesh(TropicID engine_id, const Mesh* proto);
Mesh*    Tropic_getMesh(TropicID engine_id, MeshID id);
bool     Tropic_freeMesh(TropicID engine_id, MeshID id);

/* Texture pool APIs */
TextureID Tropic_newTexture(TropicID engine_id, const Texture* proto);
Texture*  Tropic_getTexture(TropicID engine_id, TextureID id);
bool      Tropic_freeTexture(TropicID engine_id, TextureID id);

/* Shader pool APIs */
ShaderID Tropic_newShader(TropicID engine_id, const Shader* proto);
Shader*  Tropic_getShader(TropicID engine_id, ShaderID id);
bool     Tropic_freeShader(TropicID engine_id, ShaderID id);

void Tropic_enableVSync(TropicID engine_id, bool enabled);
void Tropic_enableFpsOverlay(TropicID engine_id, bool enabled);

//void Tropic_update(Tropic* self, float delta_time);
//void Tropic_render(Tropic* self);

void Tropic_cleanup(Tropic* self);

#endif /* TROPIC_H */
