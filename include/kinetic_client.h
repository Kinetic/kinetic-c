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
 * Initializes the Kinetic API and configures logging destination.
 *
 * @param log_file (path to log file, 'stdout' to log to STDOUT, NULL to disable logging)
 * @param log_level Logging level (-1:none, 0:error, 1:info, 2:verbose, 3:full)
 */
KineticClient * KineticClient_Init(const char* log_file, int log_level);

/**
 * @brief Performs shutdown/cleanup of the kinetic-c client lib
 */
void KineticClient_Shutdown(KineticClient * const client);

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
 *                  The client should call KineticClient_DestroyConnection() in
 *                  order to shutdown a connection and cleanup resources when
 *                  done using a KineticSession.
 */
KineticStatus KineticClient_CreateConnection(KineticSession * const session, KineticClient * const client);

/**
 * @brief Closes the connection to a host.
 *
 * @param session   The connected `KineticSession` to close. The connection
 *                  instance will be freed by this call after closing the
 *                  connection.
 *
 * @return          Returns the resulting KineticStatus.
 */
KineticStatus KineticClient_DestroyConnection(KineticSession * const session);

/**
 * @brief Executes a NOOP command to test whether the Kinetic Device is operational.
 *
 * @param session       The connected KineticSession to use for the operation.
 *
 * @return              Returns the resulting KineticStatus.
 */
KineticStatus KineticClient_NoOp(KineticSession const * const session);

/**
 * @brief Executes a PUT command to store/update an entry on the Kinetic Device.
 *
 * @param session       The connected KineticSession to use for the operation.
 * @param entry         Key/value entry for object to store. 'value' must
 *                      specify the data to be stored.
 * @param closure       Optional closure. If specified, operation will be
 *                      executed in asynchronous mode, and closure callback
 *                      will be called upon completion in another thread.
 * 
 * @return              Returns the resulting KineticStatus.
 */
KineticStatus KineticClient_Put(KineticSession const * const session,
                                KineticEntry* const entry,
                                KineticCompletionClosure* closure);

/**
 * @brief Executes a FLUSHALLDATA command to flush pending PUTs or DELETEs.
 *
 * @param session       The connected KineticSession to use for the operation.
 * @param closure       Optional closure. If specified, operation will be
 *                      executed in asynchronous mode, and closure callback
 *                      will be called upon completion in another thread.
 *                      
 * @return              Returns the resulting KineticStatus.
 */
KineticStatus KineticClient_Flush(KineticSession const * const session,
                                  KineticCompletionClosure* closure);

/**
 * @brief Executes a GET command to retrieve an entry from the Kinetic Device.
 *
 * @param session       The connected KineticSession to use for the operation.
 * @param entry         Key/value entry for object to retrieve. 'value' will
 *                      be populated unless 'metadataOnly' is set to 'true'.
 * @param closure       Optional closure. If specified, operation will be
 *                      executed in asynchronous mode, and closure callback
 *                      will be called upon completion in another thread.
 *
 * @return              Returns the resulting KineticStatus.
 */
KineticStatus KineticClient_Get(KineticSession const * const session,
                                KineticEntry* const entry,
                                KineticCompletionClosure* closure);

/**
 * @brief Executes a GETPREVIOUS command to retrieve the next entry from the Kinetic Device.
 *
 * @param session       The connected KineticSession to use for the operation.
 * @param entry         Key/value entry for object to retrieve. 'value' will
 *                      be populated unless 'metadataOnly' is set to 'true'.
 *                      The key and value fields will be populated with the
 *                      previous key and its corresponding value, according to
 *                      lexicographical byte order.
 *                      
 * @param closure       Optional closure. If specified, operation will be
 *                      executed in asynchronous mode, and closure callback
 *                      will be called upon completion in another thread.
 *
 * @return              Returns the resulting KineticStatus.
 */
KineticStatus KineticClient_GetPrevious(KineticSession const * const session,
                                        KineticEntry* const entry,
                                        KineticCompletionClosure* closure);

/**
 * @brief Executes a GETNEXT command to retrieve the next entry from the Kinetic Device.
 *
 * @param session       The connected KineticSession to use for the operation.
 * @param entry         Key/value entry for object to retrieve. 'value' will
 *                      be populated unless 'metadataOnly' is set to 'true'.
 *                      The key and value fields will be populated with the
 *                      next key and its corresponding value, according to
 *                      lexicographical byte order.
 *                      
 * @param closure       Optional closure. If specified, operation will be
 *                      executed in asynchronous mode, and closure callback
 *                      will be called upon completion in another thread.
 *
 * @return              Returns the resulting KineticStatus.
 */
KineticStatus KineticClient_GetNext(KineticSession const * const session,
                                    KineticEntry* const entry,
                                    KineticCompletionClosure* closure);

/**
 * @brief Executes a DELETE command to delete an entry from the Kinetic Device
 *
 * @param session       The connected KineticSession to use for the operation.
 * @param entry         Key/value entry for object to delete. 'value' is
 *                      not used for this operation.
 * @param closure       Optional closure. If specified, operation will be
 *                      executed in asynchronous mode, and closure callback
 *                      will be called upon completion in another thread.
 *
 * @return              Returns the resulting KineticStatus.
 */
KineticStatus KineticClient_Delete(KineticSession const * const session,
                                   KineticEntry* const entry,
                                   KineticCompletionClosure* closure);

/**
 * @brief Executes a GETKEYRANGE command to retrieve a set of keys in the range
 * specified range from the Kinetic Device
 *
 * @param session       The connected KineticSession to use for the operation
 * @param range         KineticKeyRange specifying keys to return
 * @param keys          ByteBufferArray to store the retrieved keys
 * @param closure       Optional closure. If specified, operation will be
 *                      executed in asynchronous mode, and closure callback
 *                      will be called upon completion in another thread.
 *
 *
 * @return              Returns 0 upon success, -1 or the Kinetic status code
 *                      upon failure
 */
KineticStatus KineticClient_GetKeyRange(KineticSession const * const session,
                                        KineticKeyRange* range, ByteBufferArray* keys,
                                        KineticCompletionClosure* closure);

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
KineticStatus KineticClient_GetLog(KineticSession const * const session,
                                   KineticDeviceInfo_Type type,
                                   KineticDeviceInfo** info,
                                   KineticCompletionClosure* closure);

/**
 * @brief Executes a PEER2PEERPUSH operation allows a client to instruct a Kinetic
 * Device to copy a set of keys (and associated value and metadata) to another
 * Kinetic Device.
 *
 * @param session       The connected KineticSession to use for the operation
 * @param p2pOp         KineticP2P_Operation pointer. This pointer needs to remain
 *                      valid during the duration of the operation. The results of 
 *                      P2P operation(s) will be stored in the resultStatus field of
 *                      this structure.
 * @param closure       Optional closure. If specified, operation will be
 *                      executed in asynchronous mode, and closure callback
 *                      will be called upon completion in another thread.
 *
 * @return              Returns 0 upon success, -1 or the Kinetic status code
 *                      upon failure. Note that P2P operations can be nested. This
 *                      status code pertains to the initial top-level P2P operation. 
 *                      You'll need to check the resultStatus in the p2pOp structure
 *                      to check the status of the individual P2P operations.
 */
KineticStatus KineticClient_P2POperation(KineticSession const * const session,
                                         KineticP2P_Operation* const p2pOp,
                                         KineticCompletionClosure* closure);

/**
 * @brief Executes an InstantSecureErase command to erase all data from the Kinetic device.
 *
 * @param session       The connected KineticSession to use for the operation.
 *
 * @return              Returns the resulting KineticStatus.
 */
KineticStatus KineticClient_InstantSecureErase(KineticSession const * const session);

/**
 * @brief Updates the cluster version.
 *
 * @param clusterVersion      Current cluster version.
 * @param newClusterVersion   New cluster version.
 *
 * @return              Returns the resulting KineticStatus.
 */
KineticStatus KineticClient_SetClusterVersion(KineticSession handle,
                                              int64_t clusterVersion,
                                              int64_t newClusterVersion);


#endif // _KINETIC_CLIENT_H
