/*
* kinetic-c
* Copyright (C) 2014 Seagate Technology.
*
* This program is free software; you can redistribute it and/or
* modify it under the terms of the GNU General Public License
* as published by the Free Software Foundation; either version 2
* of the License, or (at your option) any later version.
*
* This program is distributed in the hope that it will be useful,
* but WITHOUT ANY WARRANTY; without even the implied warranty of
* MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
* GNU General Public License for more details.
*
* You should have received a copy of the GNU General Public License
* along with this program; if not, write to the Free Software
* Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA.
*
*/

#ifndef _KINETIC_TYPES_H
#define _KINETIC_TYPES_H

// Include C99 bool definition, if not already defined
#if !defined(__bool_true_false_are_defined) || (__bool_true_false_are_defined == 0)
#include <stdbool.h>
#endif

#include "kinetic_proto.h"
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
            #ifndef _BSD_SOURCE
                #define _BSD_SOURCE
            #endif // _BSD_SOURCE
            #include <unistd.h>
        #else
            #if !defined(HOST_NAME_MAX)
                #define HOST_NAME_MAX 256
            #endif // HOST_NAME_MAX
        #endif  // __unix__
    #endif // __APPLE__
#endif  // WIN32

#define KINETIC_PORT 8213

typedef struct _KineticConnection
{
    bool    Connected;
    bool    Blocking;
    int     Port;
    int     FileDescriptor;
    char    Host[HOST_NAME_MAX];
} KineticConnection;

#endif // _KINETIC_TYPES_H
