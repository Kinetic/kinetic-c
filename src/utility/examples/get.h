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

#ifndef _GET_H
#define _GET_H

#include "kinetic_client.h"

/**
 * @brief Connects to the specified Kinetic host/port and executes a Get to read data from the Device
 *
 * @param host              Host name or IP address to connect to
 * @param port              Port to establish socket connection on
 * @param clusterVersion    Cluster version to use for the operation
 * @param identity          Identity to use for the operation (Must have ACL setup on Kinetic Device)
 * @param hmacKey           Shared secret key used for the identity for HMAC calculation
 * @param metadataOnly      Set to true to request only the meta-data for the stored object
 * @param value             ByteArray with preallocated buffer to store the received data in
 *
 * @return                  Returns true if operation succeeded, false otherwise
 */
int Get(const char* host,
        int port,
        bool nonBlocking,
        int64_t clusterVersion,
        int64_t identity,
        ByteArray hmacKey,
        Kinetic_KeyValue* metadata,
        ByteArray value);

#endif // _GET_H
