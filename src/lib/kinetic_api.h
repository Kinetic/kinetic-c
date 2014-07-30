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

#ifndef _KINETIC_API_H
#define _KINETIC_API_H

#include "kinetic_types.h"
#include "kinetic_exchange.h"
#include "kinetic_pdu.h"
#include "kinetic_operation.h"

/**
 * Initializes the Kinetic API andcsonfigures logging destination.
 *
 * @param logFile Path to log file. Specify NULL to log to STDOUT.
 */
void KineticApi_Init(
    const char* logFile);

/**
 * @brief Establishes a Kinetic protocol socket connection to a host.
 *
 * @param connection    KineticConnection instance to configure with connection info
 * @param host          Host name or IP address to connect to
 * @param port          Port to establish socket connection on
 * @param blocking      Set to true for blocking or false for non-bloocking I/O
 * @return              Returns true if connection succeeded
 */
bool KineticApi_Connect(
    KineticConnection* connection,
    const char* host,
    int port,
    bool blocking);

/**
 * @brief Initializes and configures a Kinetic exchange.
 *
 * @param exchange      KineticExchange instance to configure with exchange info
 * @param connection    KineticConnection to associate with exchange
 * @param identity      Identity to use for the exchange
 * @param key           Key to use for HMAC calculations
 * @param keyLength     Length of HMAC key
 * @return              Returns true if configuration succeeded
 */
bool KineticApi_ConfigureExchange(
    KineticExchange* exchange,
    KineticConnection* connection,
    int64_t identity,
    const char* key,
    size_t keyLength);

/**
 * @brief Creates and initializes a Kinetic operation.
 *
 * @param exchange      KineticExchange instance to populate with exchange info
 * @param request       KineticPDU instance to use for request
 * @param requestMsg    KineticMessage instance to use for request
 * @param response      KineticPDU instance to use for reponse
 * @param responseMsg   KineticMessage instance to use for reponse
 * @return              Returns a configured operation instance
 */
KineticOperation KineticApi_CreateOperation(
    KineticExchange* exchange,
    KineticPDU* request,
    KineticMessage* requestMsg,
    KineticPDU* response,
    KineticMessage* responseMsg);

/**
 * @brief Executes a NOOP command to test whether the Kinetic Device is operational
 *
 * @param operation     KineticOperation instance to use for the operation
 * @return              Returns the resultant status code
 */
KineticProto_Status_StatusCode KineticApi_NoOp(
    KineticOperation* operation
    );

#endif // _KINETIC_API_H
