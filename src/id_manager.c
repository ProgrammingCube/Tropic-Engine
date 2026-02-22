#include "../inc/id_manager.h"
#include <stdlib.h>
#include <string.h>
#include <stdint.h>

/* Internal representation */
struct IDManager {
    void **slots; /* payload pointers */
    uint16_t *gens; /* generation counters */
    uint32_t *next_free; /* free list: next index or UINT32_MAX */
    uint32_t free_head;
    uint32_t capacity;
};

static const uint32_t INVALID_INDEX = 0xFFFFFFFFu;

IDManager* idmgr_create(size_t initial_capacity)
{
    IDManager *m = (IDManager*)malloc(sizeof(IDManager));
    if (!m) return NULL;
    uint32_t cap = initial_capacity ? (uint32_t)initial_capacity : 256u;
    m->slots = (void**)calloc(cap, sizeof(void*));
    m->gens = (uint16_t*)calloc(cap, sizeof(uint16_t));
    m->next_free = (uint32_t*)malloc(cap * sizeof(uint32_t));
    if (!m->slots || !m->gens || !m->next_free) {
        free(m->slots); free(m->gens); free(m->next_free); free(m);
        return NULL;
    }
    for (uint32_t i = 0; i < cap; ++i) m->next_free[i] = i + 1;
    m->next_free[cap-1] = INVALID_INDEX;
    m->free_head = 0;
    m->capacity = cap;
    return m;
}

void idmgr_destroy(IDManager* mgr)
{
    if (!mgr) return;
    free(mgr->slots);
    free(mgr->gens);
    free(mgr->next_free);
    free(mgr);
}

/* Encode handle: (gen << 16) | index */
/* Encode handle: low 16 bits store (index+1) so that handle 0 is always invalid */
static inline Handle encode_handle(uint32_t index, uint16_t gen)
{
    uint32_t stored = (index + 1u) & 0xFFFFu;
    return ((Handle)gen << 16) | (Handle)stored;
}

static inline uint32_t handle_index(Handle h)
{
    uint32_t stored = (uint32_t)(h & 0xFFFFu);
    if (stored == 0) return INVALID_INDEX;
    return stored - 1u;
}

static inline uint16_t handle_gen(Handle h)
{
    return (uint16_t)(h >> 16);
}

/* Grow capacity; returns 0 on failure, 1 on success */
static int grow(IDManager* m)
{
    uint32_t old = m->capacity;
    uint32_t newcap = old * 2u;
    void **s2 = (void**)realloc(m->slots, newcap * sizeof(void*));
    uint16_t *g2 = (uint16_t*)realloc(m->gens, newcap * sizeof(uint16_t));
    uint32_t *n2 = (uint32_t*)realloc(m->next_free, newcap * sizeof(uint32_t));
    if (!s2 || !g2 || !n2) return 0;
    m->slots = s2; m->gens = g2; m->next_free = n2;
    for (uint32_t i = old; i < newcap; ++i) m->next_free[i] = i + 1;
    m->next_free[newcap-1] = m->free_head;
    m->free_head = old;
    m->capacity = newcap;
    return 1;
}

Handle idmgr_alloc(IDManager* mgr, void* payload)
{
    if (!mgr) return 0;
    if (mgr->free_head == INVALID_INDEX) {
        if (!grow(mgr)) return 0;
    }
    uint32_t idx = mgr->free_head;
    mgr->free_head = mgr->next_free[idx];
    mgr->slots[idx] = payload;
    uint16_t gen = mgr->gens[idx];
    return encode_handle(idx, gen);
}

void* idmgr_get(IDManager* mgr, Handle h)
{
    if (!mgr || h == 0) return NULL;
    uint32_t idx = handle_index(h);
    if (idx >= mgr->capacity) return NULL;
    uint16_t gen = handle_gen(h);
    if (mgr->gens[idx] != gen) return NULL;
    return mgr->slots[idx];
}

bool idmgr_free(IDManager* mgr, Handle h)
{
    if (!mgr || h == 0) return false;
    uint32_t idx = handle_index(h);
    if (idx >= mgr->capacity) return false;
    uint16_t gen = handle_gen(h);
    if (mgr->gens[idx] != gen) return false;
    /* clear payload */
    mgr->slots[idx] = NULL;
    /* increment generation (wrap-around allowed) */
    mgr->gens[idx] = (uint16_t)(mgr->gens[idx] + 1u);
    /* push to free list */
    mgr->next_free[idx] = mgr->free_head;
    mgr->free_head = idx;
    return true;
}

bool idmgr_is_valid(IDManager* mgr, Handle h)
{
    if (!mgr || h == 0) return false;
    uint32_t idx = handle_index(h);
    if (idx >= mgr->capacity) return false;
    uint16_t gen = handle_gen(h);
    return mgr->gens[idx] == gen && mgr->slots[idx] != NULL;
}

Handle idmgr_get_by_payload(IDManager* mgr, void* payload)
{
    if (!mgr || !payload) return 0;
    for (uint32_t i = 0; i < mgr->capacity; ++i) {
        if (mgr->slots[i] == payload) {
            uint16_t gen = mgr->gens[i];
            return encode_handle(i, gen);
        }
    }
    return 0;
}

void idmgr_free_all(IDManager* mgr, void (*free_fn)(void*))
{
    if (!mgr) return;
    if (free_fn) {
        for (uint32_t i = 0; i < mgr->capacity; ++i) {
            if (mgr->slots[i]) {
                free_fn(mgr->slots[i]);
                mgr->slots[i] = NULL;
            }
        }
    }
    idmgr_destroy(mgr);
}
