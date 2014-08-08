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

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include "kinetic.h"
#include "noop.h"

int main(int argc, char** argv)
{
    int status = -1;
    char* host = "localhost";
    int port = KINETIC_PORT;
    const int64_t clusterVersion = 0;
    const int64_t identity = 1;
    const char* key = "asdfasdf";

    printf("\nExecuting NoOp w/configuration:"
           "\n-------------------------------\n"
           "\n  host: %s\n  port: %d"
           "\n  clusterVersion: %lld\n  identity: %lld\n  key: '%s'\n", 
        host, port, (long long int)clusterVersion, (long long int)identity, key);
    status = NoOp(host, port, clusterVersion, identity, key);
    if (status == 0)
    {
        printf("\nNoOp executed successfully!\n\n");
    }
    else
    {
        printf("\nNoOp operation failed! status=%d\n\n", status);
    }
    
    return status;
}
