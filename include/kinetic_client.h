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
 * @param log_file (path to log file, 'stdout' to log to STDOUT, NULL to disable logging)
 * @param log_level Logging level (-1:none, 0:error, 1:info, 2:verbose, 3:full)
 */
void KineticClient_Init(const char* log_file, int log_level);

/**
 * @brief Performs shutdown/cleanup of the kinetic-c client lib
 */
void KineticClient_Shutdown(void);

/**
 * @brief Initializes the Kinetic API, configures logging destination, establishes a
 * connection to the specified Kinetic Device, and establishes a session.
 *
 * @param config    Session configuration
 *  .host             Host name or IP address to connect to
 *  .port             Port to establish socket connection on
 *  .clusterVersion   Cluster version to use for the session
 *  .identity         Identity to use for the session
 *  .hmacKey          Key to use for HMAC calculations (NULL-terminated string)
 * @param handle    Pointer to KineticSessionHandle (populated upon successful connection)
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
 * @param entry         Key/value entry for object to store. 'value' must
 *                      specify the data to be stored.
 * @param closure       Optional closure. If specified, operation will be
 *                      executed in asynchronous mode, and closure callback
 *                      will be called upon completion.
 * 
 *
 * @return              Returns the resulting KineticStatus
 */
KineticStatus KineticClient_Put(KineticSessionHandle handle,
                                KineticEntry* const entry,
                                KineticCompletionClosure* closure);

/**
 * @brief Executes a FLUSHALLDATA command to flush pending PUTs or DELETEs.
 *
 * @param handle        KineticSessionHandle for a connected session.
 * @param closure       Optional closure. If specified, operation will be
 *                      executed in asynchronous mode, and closure callback
 *                      will be called upon completion.
 *                      
 * @return              Returns the resulting KineticStatus
 */
KineticStatus KineticClient_Flush(KineticSessionHandle handle,
                                  KineticCompletionClosure* closure);

/**
 * @brief Executes a GET command to retrieve and entry from the Kinetic Device.
 *
 * @param handle        KineticSessionHandle for a connected session.
 * @param entry         Key/value entry for object to retrieve. 'value' will
 *                      be populated unless 'metadataOnly' is set to 'true'.
 * @param closure       Optional closure. If specified, operation will be
 *                      executed in asynchronous mode, and closure callback
 *                      will be called upon completion.
 *
 * @return              Returns the resulting KineticStatus
 */
KineticStatus KineticClient_Get(KineticSessionHandle handle,
                                KineticEntry* const entry,
                                KineticCompletionClosure* closure);

/**
 * @brief Executes a DELETE command to delete an entry from the Kinetic Device
 *
 * @param handle        KineticSessionHandle for a connected session.
 * @param entry         Key/value entry for object to delete. 'value' is
 *                      not used for this operation.
 * @param closure       Optional closure. If specified, operation will be
 *                      executed in asynchronous mode, and closure callback
 *                      will be called upon completion.
 *
 * @return              Returns the resulting KineticStatus
 */
KineticStatus KineticClient_Delete(KineticSessionHandle handle,
                                   KineticEntry* const entry,
                                   KineticCompletionClosure* closure);

/**
 * @brief Executes a GETKEYRANGE command to retrive a set of keys in the range
 * specified range from the Kinetic Device
 *
 * @param handle        KineticSessionHandle for a connected session
 * @param range         KineticKeyRange specifying keys to return
 * @param keys          ByteBufferArray to store the retrieved keys
 * @param closure       Optional closure. If specified, operation will be
 *                      executed in asynchronous mode, and closure callback
 *                      will be called upon completion.
 *
 *
 * @return              Returns 0 upon succes, -1 or the Kinetic status code
 *                      upon failure
 */
KineticStatus KineticClient_GetKeyRange(KineticSessionHandle handle,
                                        KineticKeyRange* range, ByteBufferArray* keys,
                                        KineticCompletionClosure* closure);

/**
 * @brief Executes a GETLOG command to retrive a set of keys in the range
 * specified range from the Kinetic Device
 *
 * @param handle        KineticSessionHandle for a connected session
 * @param type          KineticLogDataType specifying data type to retrieve.
 * @param info          KineticDeviceInfo pointer, which will be assigned to
 *                      a dynamically allocated structure populated with
 *                      the requested data, if successful.
 * @param closure       Optional closure. If specified, operation will be
 *                      executed in asynchronous mode, and closure callback
 *                      will be called upon completion.
 *
 * @return              Returns 0 upon succes, -1 or the Kinetic status code
 *                      upon failure
 */
KineticStatus KineticClient_GetLog(KineticSessionHandle handle,
                                   KineticDeviceInfo_Type type,
                                   KineticDeviceInfo** info,
                                   KineticCompletionClosure* closure);

/**
 * @brief Executes an InstantSecureErase command to erase all data from the Kinetic device.
 *
 * @param handle        KineticSessionHandle for a connected session.
 *
 * @return              Returns the resulting KineticStatus
 */
KineticStatus KineticClient_InstantSecureErase(KineticSessionHandle handle);

#endif // _KINETIC_CLIENT_H
