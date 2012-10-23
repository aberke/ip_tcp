#ifndef __EXT_ARRAY_H__ 
#define __EXT_ARRAY_H__

#define SCALE_FACTOR 2

#include "utils.h"

typedef struct ext_array* ext_array_t;

ext_array_t ext_array_init( /* ARGUMENTS */ );
void ext_array_destroy(ext_array_t* ext_array);

void ext_array_push(ext_array_t ar, void* data, int length);
memchunk_t ext_array_peel(ext_array_t ar, int desired_length);

#endif // __EXT_ARRAY_H__
