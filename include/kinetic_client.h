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
 * @brief Initializes the Kinetic API, configures logging destination, establishes a
 * connection to the specified Kinetic Device, and establishes a session.
 *
 * @config          Session configuration
 *  .host               Host name or IP address to connect to
 *  .port               Port to establish socket connection on
 *  .nonBlocking        Set to true for non-blocking or false for blocking I/O
 *  .clusterVersion     Cluster version to use for the session
 *  .identity           Identity to use for the session
 *  .hmacKey            Key to use for HMAC calculations (NULL-terminated string)
 *  .logFile            Path to log file. Defaults to STDOUT if unspecified
 * @handle          Pointer to KineticSessionHandle (populated upon successful connection)
 *
 * @return          Returns the resulting KineticStatus
 */
KineticStatus KineticClient_Connect(const KineticSession* config,
    KineticSessionHandle* const handle);

/**
 * @brief Closes the connection to a host.
 *
 * @param handle    KineticSessionHandle for a connected session.
 *
 * @return          Returns the resulting KineticStatus
 */
KineticStatus KineticClient_Disconnect(KineticSessionHandle* const handle);

/**
 * @brief Executes a NOOP command to test whether the Kinetic Device is operational.
 *
 * @param handle        KineticSessionHandle for a connected session.
 *
 * @return              Returns the resulting KineticStatus
 */
KineticStatus KineticClient_NoOp(KineticSessionHandle session);

/**
 * @brief Executes a PUT command to store/update an entry on the Kinetic Device.
 *
 * @param handle        KineticSessionHandle for a connected session.
 * @param metadata      Key/value metadata for object to store. 'value' must
 *                      specify the data to be stored.
 *
 * @return              Returns the resulting KineticStatus
 */
KineticStatus KineticClient_Put(KineticSessionHandle session,
    KineticKeyValue* const metadata);

/**
 * @brief Executes a GET command to retrieve and entry from the Kinetic Device.
 *
 * @param handle        KineticSessionHandle for a connected session.
 * @param metadata      Key/value metadata for object to retrieve. 'value' will
 *                      be populated unless 'metadataOnly' is set to 'true'
 *
 * @return              Returns the resulting KineticStatus
 */
KineticStatus KineticClient_Get(KineticSessionHandle session,
    KineticKeyValue* const metadata);

/**
 * @brief Executes a DELETE command to delete an entry from the Kinetic Device
 *
 * @param handle        KineticSessionHandle for a connected session.
 * @param metadata      Key/value metadata for object to delete. 'value' is
 *                      not used for this operation.
 *
 * @return              Returns the resulting KineticStatus
 */
KineticStatus KineticClient_Delete(KineticSessionHandle session,
    KineticKeyValue* const metadata);

#endif // _KINETIC_CLIENT_H
