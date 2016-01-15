/**
 * Copyright 2013-2015 Seagate Technology LLC.
 *
 * This Source Code Form is subject to the terms of the Mozilla
 * Public License, v. 2.0. If a copy of the MPL was not
 * distributed with this file, You can obtain one at
 * https://mozilla.org/MP:/2.0/.
 *
 * This program is distributed in the hope that it will be useful,
 * but is provided AS-IS, WITHOUT ANY WARRANTY; including without
 * the implied warranty of MERCHANTABILITY, NON-INFRINGEMENT or
 * FITNESS FOR A PARTICULAR PURPOSE. See the Mozilla Public
 * License for more details.
 *
 * See www.openkinetic.org for more project information
 */

#ifndef _KINETIC_CLIENT_H
#define _KINETIC_CLIENT_H

#include "kinetic_types.h"

/**
 * @brief Gets current version info of kinetic-c library
 *
 * @return Returns a pointer to static version info
 */
const KineticVersionInfo * KineticClient_Version(void);

/**
 * Initializes the Kinetic API and configures logging.
 *
 * @param config A configuration struct.
 *
 * @return          Returns a pointer to a KineticClient. You need to pass
 *                  this pointer to KineticClient_CreateSession() to create
 *                  new connections.
 *                  Once you are finished will the KineticClient, and there
 *                  are no active connections. The pointer should be release
 *                  with KineticClient_Shutdown()
 */
KineticClient * KineticClient_Init(KineticClientConfig *config);

/**
 * @brief Performs shutdown/cleanup of the kinetic-c client library
 *
 * @param client The pointer returned from `KineticClient_Init`
 *
 */
void KineticClient_Shutdown(KineticClient * const client);

/**
 * @brief Creates a session with the Kinetic Device per specified configuration.
 *
 * @param config   KineticSessionConfig structure which must be configured
 *                 by the client prior to creating the device connection.
 *   .host             Host name or IP address to connect to
 *   .port             Port to establish socket connection on
 *   .clusterVersion   Cluster version to use for the session
 *   .identity         Identity to use for the session
 *   .hmacKey          Key to use for HMAC calculations (NULL-terminated string)
 *   .pin              PIN to use for PIN-based operations
 * @param client    The KineticClient pointer returned from KineticClient_Init()
 * @param session   Pointer to a KineticSession pointer that will be populated
 *                  with the allocated/created session upon success.
 *
 * @return          Returns the resulting KineticStatus, and `session`
 *                  will be populated with a pointer to the session instance
 *                  upon success. The client should call
 *                  KineticClient_DestroySession() in order to shutdown a
 *                  connection and cleanup resources when done using a
 *                  KineticSession.
 */
KineticStatus KineticClient_CreateSession(KineticSessionConfig * const config,
    KineticClient * const client, KineticSession** session);

/**
 * @brief Closes the connection to a host.
 *
 * @param session   The connected KineticSession to close. The session
 *                  instance will be freed by this call after closing the
 *                  connection, so the pointer should not longer be used.
 *
 * @return          Returns the resulting KineticStatus.
 */
KineticStatus KineticClient_DestroySession(KineticSession * const session);

/**
 * @brief Returns the reason reported in the case of the Kinetic device
 * terminating a session in the case of a catastrophic error occurring.
 *
 * @param session       The KineticSession to query.
 *
 * @return              Returns the status reported prior to termination
 *                      or KINTEIC_STATUS_SUCCESS if not terminated.
 */
KineticStatus KineticClient_GetTerminationStatus(KineticSession * const session);

/**
 * @brief Executes a `NOOP` operation to test whether the Kinetic Device is operational.
 *
 * @param session       The connected KineticSession to use for the operation.
 *
 * @return              Returns the resulting KineticStatus.
 */
KineticStatus KineticClient_NoOp(KineticSession* const session);

/**
 * @brief Executes a `PUT` operation to store/update an entry on the Kinetic Device.
 *
 * @param session       The connected KineticSession to use for the operation.
 * @param entry         Key/value entry for object to store. 'value' must
 *                      specify the data to be stored. If a closure is provided
 *                      this pointer must remain valid until the closure callback
 *                      is called.
 *
 * @param closure       Optional closure. If specified, operation will be
 *                      executed in asynchronous mode, and closure callback
 *                      will be called upon completion in another thread.
 *
 * @return              Returns the resulting KineticStatus.
 */
KineticStatus KineticClient_Put(KineticSession* const session,
                                KineticEntry* const entry,
                                KineticCompletionClosure* closure);

/**
 * @brief Executes a `FLUSHALLDATA` operation to flush pending PUTs or DELETEs.
 *
 * @param session       The connected KineticSession to use for the operation.
 * @param closure       Optional closure. If specified, operation will be
 *                      executed in asynchronous mode, and closure callback
 *                      will be called upon completion in another thread.
 *
 * @return              Returns the resulting KineticStatus.
 */
KineticStatus KineticClient_Flush(KineticSession* const session,
                                  KineticCompletionClosure* closure);

/**
 * @brief Executes a `GET` operation to retrieve an entry from the Kinetic Device.
 *
 * @param session       The connected KineticSession to use for the operation.
 * @param entry         Key/value entry for object to retrieve. 'value' will
 *                      be populated unless 'metadataOnly' is set to 'true'.
 *                      If a closure is provided this pointer must remain
 *                      valid until the closure callback is called.
 * @param closure       Optional closure. If specified, operation will be
 *                      executed in asynchronous mode, and closure callback
 *                      will be called upon completion in another thread.
 *
 * @return              Returns the resulting KineticStatus.
 */
KineticStatus KineticClient_Get(KineticSession* const session,
                                KineticEntry* const entry,
                                KineticCompletionClosure* closure);

/**
 * @brief Executes a `GETPREVIOUS` operation to retrieve the next entry from the Kinetic Device.
 *
 * @param session       The connected KineticSession to use for the operation.
 * @param entry         Key/value entry for object to retrieve. 'value' will
 *                      be populated unless 'metadataOnly' is set to 'true'.
 *                      The key and value fields will be populated with the
 *                      previous key and its corresponding value, according to
 *                      lexicographical byte order. If a closure is provided
 *                      this pointer must remain valid until the closure callback
 *                      is called.
 *
 * @param closure       Optional closure. If specified, operation will be
 *                      executed in asynchronous mode, and closure callback
 *                      will be called upon completion in another thread.
 *
 * @return              Returns the resulting KineticStatus.
 */
KineticStatus KineticClient_GetPrevious(KineticSession* const session,
                                        KineticEntry* const entry,
                                        KineticCompletionClosure* closure);

/**
 * @brief Executes a `GETNEXT` operation to retrieve the next entry from the Kinetic Device.
 *
 * @param session       The connected KineticSession to use for the operation.
 * @param entry         Key/value entry for object to retrieve. 'value' will
 *                      be populated unless 'metadataOnly' is set to 'true'.
 *                      The key and value fields will be populated with the
 *                      next key and its corresponding value, according to
 *                      lexicographical byte order. If a closure is provided
 *                      this pointer must remain valid until the closure callback
 *                      is called.
 *
 * @param closure       Optional closure. If specified, operation will be
 *                      executed in asynchronous mode, and closure callback
 *                      will be called upon completion in another thread.
 *
 * @return              Returns the resulting KineticStatus.
 */
KineticStatus KineticClient_GetNext(KineticSession* const session,
                                    KineticEntry* const entry,
                                    KineticCompletionClosure* closure);

/**
 * @brief Executes a `DELETE` operation to delete an entry from the Kinetic Device
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
KineticStatus KineticClient_Delete(KineticSession* const session,
                                   KineticEntry* const entry,
                                   KineticCompletionClosure* closure);

/**
 * @brief Executes a `GETKEYRANGE` operation to retrieve a set of keys in the range
 * specified range from the Kinetic Device
 *
 * @param session       The connected KineticSession to use for the operation
 * @param range         KineticKeyRange specifying keys to return
 * @param keys          ByteBufferArray to store the retrieved keys. If a
 *                      closure is provided, this must point to valid memory
 *                      until the closure callback is called.
 * @param closure       Optional closure. If specified, operation will be
 *                      executed in asynchronous mode, and closure callback
 *                      will be called upon completion in another thread.
 *
 *
 * @return              Returns 0 upon success, -1 or the Kinetic status code
 *                      upon failure
 */
KineticStatus KineticClient_GetKeyRange(KineticSession* const session,
                                        KineticKeyRange* range, ByteBufferArray* keys,
                                        KineticCompletionClosure* closure);

/**
 * @brief Executes a `PEER2PEERPUSH` operation allows a client to instruct a Kinetic
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
KineticStatus KineticClient_P2POperation(KineticSession* const session,
                                         KineticP2P_Operation* const p2pOp,
                                         KineticCompletionClosure* closure);

#endif // _KINETIC_CLIENT_H
