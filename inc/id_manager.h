#ifndef ID_MANAGER_H
#define ID_MANAGER_H

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>

/* Handle is a 32-bit value: [generation:16][index:16] */
typedef uint32_t Handle;
typedef uint64_t Handle64;

typedef struct IDManager IDManager;

/* Create an ID manager with initial capacity (0 for default). */
IDManager* idmgr_create(size_t initial_capacity);
void idmgr_destroy(IDManager* mgr);

/* Allocate a handle and associate it with a payload pointer. Returns 0 on failure (invalid handle). */
Handle idmgr_alloc(IDManager* mgr, void* payload);

/* Retrieve payload for a handle, or NULL if invalid/stale. */
void* idmgr_get(IDManager* mgr, Handle h);

/* Free a handle. Returns true if freed, false if handle was invalid. */
bool idmgr_free(IDManager* mgr, Handle h);

/* Check if a handle is valid (not stale). */
bool idmgr_is_valid(IDManager* mgr, Handle h);

/* Retrieve handle for a payload pointer, or 0 if not found. */
Handle idmgr_get_by_payload(IDManager* mgr, void* payload);

/* Free all payloads using provided function, then destroy the manager. */
void idmgr_free_all(IDManager* mgr, void (*free_fn)(void*));

#endif /* ID_MANAGER_H */
