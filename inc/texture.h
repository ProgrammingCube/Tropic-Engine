#ifndef TEXTURE_H
#define TEXTURE_H

#include <stdint.h>

typedef struct sTexture {
    uint32_t id;
    /* Placeholder for texture handle, format, dimensions, etc. */
    void *user;
} Texture;

#endif /* TEXTURE_H */
