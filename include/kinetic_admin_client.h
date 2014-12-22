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

#ifndef _KINETIC_ADMIN_CLIENT_H
#define _KINETIC_ADMIN_CLIENT_H

#include "kinetic_types.h"
#include "kinetic_client.h"

/**
 * Initializes the Kinetic API and configures logging destination.
 *
 * @param log_file (path to log file, 'stdout' to log to STDOUT, NULL to disable logging)
 * @param log_level Logging level (-1:none, 0:error, 1:info, 2:verbose, 3:full)
 */
void KineticAdminClient_Init(const char* log_file, int log_level);

/**
 * @brief Performs shutdown/cleanup of the kinetic-c client lib
 */
void KineticAdminClient_Shutdown(void);

/**
 * @brief Initializes the Kinetic API, configures logging destination, establishes a
 * connection to the specified Kinetic Device, and establishes a session.
 *
 * @param session   Configured KineticSession to connect
 *  .config           `KineticSessionConfig` structure which must be configured
 *                    by the client prior to creating the device connection.
 *    .host             Host name or IP address to connect to
 *    .port             Port to establish socket connection on
 *    .clusterVersion   Cluster version to use for the session
 *    .identity         Identity to use for the session
 *    .hmacKey          Key to use for HMAC calculations (NULL-terminated string)
 *  .connection     Pointer to dynamically allocated connection which will be
 *                  populated upon establishment of a connection.
 *
 * @return          Returns the resulting `KineticStatus`, and `session->connection`
 *                  will be populated with a session instance pointer upon success.
 *                  The client should call KineticAdminClient_DestroyConnection() in
 *                  order to shutdown a connection and cleanup resources when
 *                  done using a KineticSession.
 */
KineticStatus KineticAdminClient_CreateConnection(KineticSession * const session);

/**
 * @brief Closes the connection to a host.
 *
 * @param session   The connected `KineticSession` to close. The connection
 *                  instance will be freed by this call after closing the
 *                  connection.
 *
 * @return          Returns the resulting KineticStatus.
 */
KineticStatus KineticAdminClient_DestroyConnection(KineticSession * const session);

/**
 * @brief Executes a GETLOG command to retrieve specific configuration and/or
 * operational data from the Kinetic Device.
 *
 * @param session       The connected KineticSession to use for the operation
 * @param type          KineticLogDataType specifying data type to retrieve.
 * @param info          KineticDeviceInfo pointer, which will be assigned to
 *                      a dynamically allocated structure populated with
 *                      the requested data, if successful. The client should
 *                      call free() on this pointer in order to free the root
 *                      and any nested structures.
 * @param closure       Optional closure. If specified, operation will be
 *                      executed in asynchronous mode, and closure callback
 *                      will be called upon completion in another thread.
 *
 * @return              Returns 0 upon success, -1 or the Kinetic status code
 *                      upon failure
 */
KineticStatus KineticAdminClient_GetLog(KineticSession const * const session,
                                   KineticDeviceInfo_Type type,
                                   KineticDeviceInfo** info,
                                   KineticCompletionClosure* closure);

// def setClusterVersion(self, *args, **kwargs):

// def updateFirmware(self, *args, **kwargs):

// @withPin @requiresSsl
// def unlock(self, *args, **kwargs):

// @withPin @requiresSsl
// def lock(self, *args, **kwargs):

// @withPin @requiresSsl
// def erase(self, *args, **kwargs):

// @withPin @requiresSsl
// def instantSecureErase(self, *args, **kwargs):

// @requiresSsl
// Set the access control lists to lock users out of different permissions.
// Arguments: aclList -> A list of ACL (Access Control List) objects.
// def setSecurity(self, *args, **kwargs):

/**
 * @brief Executes an InstantSecureErase command to erase all data from the Kinetic device.
 *
 * @param session       The connected KineticSession to use for the operation.
 *
 * @return              Returns the resulting KineticStatus.
 */
KineticStatus KineticAdminClient_InstantSecureErase(KineticSession const * const session);

/**
 * @brief Updates the cluster version.
 *
 * @param clusterVersion      Current cluster version.
 * @param newClusterVersion   New cluster version.
 *
 * @return              Returns the resulting KineticStatus.
 */
KineticStatus KineticAdminClient_SetClusterVersion(KineticSession const * const session,
                                              int64_t clusterVersion,
                                              int64_t newClusterVersion);

#endif // _KINETIC_ADMIN_CLIENT_H
