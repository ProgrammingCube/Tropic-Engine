#ifndef TEXTURE_H
#define TEXTURE_H

#include "handles.h"

typedef struct sTexture {
    TextureID id;
    /* Placeholder for texture handle, format, dimensions, etc. */
    void *user;
} Texture;

#endif /* TEXTURE_H */
