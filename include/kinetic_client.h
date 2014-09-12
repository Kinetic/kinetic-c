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

#ifndef _KINETIC_CLIENT_H
#define _KINETIC_CLIENT_H

#include "kinetic_types.h"

/**
 * Initializes the Kinetic API, configures logging destination, establishes a
 * connection to the specified Kinetic Device, and establishes a session.
 *
 * @session Session instance to configure with connection info
 *  .host           Host name or IP address to connect to
 *  .port           Port to establish socket connection on
 *  .nonBlocking    Set to true for non-blocking or false for blocking I/O
 *  .clusterVersion Cluster version to use for the session
 *  .identity       Identity to use for the session
 *  .hmacKey        Key to use for HMAC calculations (NULL-terminated string)
 *  .logFile        Path to log file. Defaults to STDOUT if unspecified
 *
 * @return          Returns 0 upon succes, -1 or Kinetic status upon failure
 */
int KineticClient_Connect(KineticSession* session);

/**
 * @brief Closes the connection to a host.
 *
 * @param session   KineticSession instance to terminate
 */
void KineticClient_Disconnect(KineticSession* session);

/**
 * @brief Executes a NOOP command to test whether the Kinetic Device is operational
 *
 * @param operation     KineticOperation instance to use for the operation
 *
 * @return              Returns 0 upon succes, -1 or the Kinetic status code upon failure
 */
Kinetic_Status KineticClient_NoOp(KineticSession* session);

/**
 * @brief Executes a PUT command to store/update an entry on the Kinetic Device
 *
 * @param operation     KineticOperation instance to use for the operation
 * @param metadata      Key/value metadata for object to store. 'value' must
 *                      specify the data to be stored.
 *
 * @return              Returns 0 upon succes, -1 or the Kinetic status code
 *                      upon failure
 */
Kinetic_Status KineticClient_Put(KineticSession* session,
    const Kinetic_KeyValue* metadata);

/**
 * @brief Executes a DELETE command to delete an entry from the Kinetic Device
 *
 * @param operation     KineticOperation instance to use for the operation
 * @param metadata      Key/value metadata for object to delete. 'value' is
 *                      not used for this operation.
 *
 * @return              Returns 0 upon succes, -1 or the Kinetic status code
 *                      upon failure
 */
KineticStatus KineticClient_Delete(KineticSession* session,
    KineticKeyValue* metadata);

#endif // _KINETIC_CLIENT_H
