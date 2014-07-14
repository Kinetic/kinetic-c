#ifndef _KINETIC_TYPES_H
#define _KINETIC_TYPES_H

// Include C99 bool definition, if not already defined
#if !defined(__bool_true_false_are_defined) || (__bool_true_false_are_defined == 0)
#include <stdbool.h>
#endif

#include "KineticProto.h"

#include <netinet/in.h>
#include <ifaddrs.h>

// Windows doesn't use <unistd.h> nor does it define HOST_NAME_MAX.
#if defined(WIN32)
    #include <windows.h>
    #include <winsock2.h>
    #define HOST_NAME_MAX (MAX_COMPUTERNAME_LENGTH+1)
#else
    #if defined(__APPLE__)
        // MacOS does not defined HOST_NAME_MAX so fall back to the POSIX value.
        // We are supposed to use sysconf(_SC_HOST_NAME_MAX) but we use this value
        // to size an automatic array and we can't portably use variables for that.
        #define HOST_NAME_MAX _POSIX_HOST_NAME_MAX
    #else
        #if defined(__unix__)
            // Some Linux environments require this, although not all, but it's benign.
            #define _BSD_SOURCE
            #include <unistd.h>
        #else
            #if !defined(HOST_NAME_MAX)
                #define HOST_NAME_MAX 256
            #endif // HOST_NAME_MAX
        #endif  // __unix__
    #endif // __APPLE__
#endif  // WIN32

#define KINETIC_PORT 8213

#endif // _KINETIC_TYPES_H
