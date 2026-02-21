#ifndef MESH_H
#define MESH_H

#include <stdint.h>

typedef struct sMesh {
    uint32_t id;
    /* Placeholder for mesh data (vertex buffers, indices, etc.) */
    void *user; /* user pointer for later extension */
} Mesh;

#endif /* MESH_H */
