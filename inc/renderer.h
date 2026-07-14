#ifndef RENDERER_H
#define RENDERER_H

#include <stdbool.h>
#include "handles.h"

void Tropic_Render( TropicID engine_id );
void Tropic_enableBeatGridDebug(TropicID engine_id, bool enabled);
bool Tropic_isBeatGridDebugEnabled(TropicID engine_id);

#endif /* RENDERER_H */
