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
* Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA.
*
*/

#ifndef _KINETIC_CLIENT_H
#define _KINETIC_CLIENT_H

#include "kinetic_types.h"

/**
 * Initializes the Kinetic API andcsonfigures logging destination.
 *
 * @param logFile Path to log file. Specify NULL to log to STDOUT.
 */
void KineticClient_Init(const char* logFile);

/**
 * @brief Performs shutdown/cleanup of the kinetic-c client lib
 */
void KineticClient_Shutdown(void);

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
 * @handle          Pointer to KineticSessionHandle (populated upon successful connection)
 *
 * @return          Returns the resulting KineticStatus
 */
KineticStatus KineticClient_Connect(const KineticSession* config,
                                    KineticSessionHandle* handle);

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
KineticStatus KineticClient_NoOp(KineticSessionHandle handle);

/**
 * @brief Executes a PUT command to store/update an entry on the Kinetic Device.
 *
 * @param handle        KineticSessionHandle for a connected session.
 * @param metadata      Key/value metadata for object to store. 'value' must
 *                      specify the data to be stored.
 *
 * @return              Returns the resulting KineticStatus
 */
KineticStatus KineticClient_Put(KineticSessionHandle handle,
                                KineticEntry* const metadata);

/**
 * @brief Executes a GET command to retrieve and entry from the Kinetic Device.
 *
 * @param handle        KineticSessionHandle for a connected session.
 * @param metadata      Key/value metadata for object to retrieve. 'value' will
 *                      be populated unless 'metadataOnly' is set to 'true'
 *
 * @return              Returns the resulting KineticStatus
 */
KineticStatus KineticClient_Get(KineticSessionHandle handle,
                                KineticEntry* const metadata);

/**
 * @brief Executes a DELETE command to delete an entry from the Kinetic Device
 *
 * @param handle        KineticSessionHandle for a connected session.
 * @param metadata      Key/value metadata for object to delete. 'value' is
 *                      not used for this operation.
 *
 * @return              Returns the resulting KineticStatus
 */
KineticStatus KineticClient_Delete(KineticSessionHandle handle,
                                   KineticEntry* const metadata);

/**
 * @brief Executes a GETKEYRANGE command to retrive a set of keys in the range
 * specified range from the Kinetic Device
 *
 * @param handle        KineticSessionHandle for a connected session.
 * @param range         KineticKeyRange specifying keys to return
 * @param keys          An pointer to an array of ByteBuffers with pre-allocated
 *                      arrays to store the retrieved keys
 * @param max_keys      The number maximum number of keys to request from the
 *                      device. There must be at least this many ByteBuffers in
 *                      the `keys` array for population.
 *
 *
 * @return              Returns 0 upon succes, -1 or the Kinetic status code
 *                      upon failure
 */
KineticStatus KineticClient_GetKeyRange(KineticSessionHandle handle,
                                        KineticKeyRange* range, ByteBuffer keys[], int max_keys);

#endif // _KINETIC_CLIENT_H
