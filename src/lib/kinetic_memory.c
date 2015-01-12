#include "kinetic_memory.h"

#include <stdlib.h>

void * KineticCalloc(size_t count, size_t size)
{
    return calloc(count, size);
}

void KineticFree(void * pointer)
{
    free(pointer);
}
