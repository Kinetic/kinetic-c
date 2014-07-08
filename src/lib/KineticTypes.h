#ifndef _KINETIC_TYPES_H
#define _KINETIC_TYPES_H

// Include C99 bool definition, if not already defined
#if !defined(__bool_true_false_are_defined) || (__bool_true_false_are_defined == 0)
#include <stdbool.h>
#endif

#include "KineticProto.h"

#include <netinet/in.h>
#include <ifaddrs.h>

#define HOST_NAME_MAX 256

typedef struct _KineticConnection
{
    bool    Connected;
    bool    Blocking;
    int     Port;
    int     FileDescriptor;
    char    Host[HOST_NAME_MAX];
} KineticConnection;

#endif // _KINETIC_TYPES_H
