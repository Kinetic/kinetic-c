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

#include "kinetic_allocator.h"
#include "kinetic_logger.h"
#include "kinetic_memory.h"
#include "kinetic_resourcewaiter.h"
#include "kinetic_resourcewaiter_types.h"
#include <stdlib.h>
#include <pthread.h>


KineticSession* KineticAllocator_NewSession(struct bus * b, KineticSessionConfig* config)
{
    (void)b; // TODO: combine session w/connection, which will use this variable

    // Allocate a new session
    KineticSession* session = KineticCalloc(1, sizeof(KineticSession));
    if (session == NULL) {
        LOG0("Failed allocating a new session!");
        return NULL;
    }

    // Deep copy the supplied config internally
    session->config = *config;
    memcpy(session->config.keyData, config->hmacKey.data, config->hmacKey.len);
    session->config.hmacKey.data = session->config.keyData;
    strncpy(session->config.host, config->host, sizeof(session->config.host));
    session->timeoutSeconds = config->timeoutSeconds; // TODO: Eliminate this, since already in config?
    KineticResourceWaiter_Init(&session->connectionReady);
    session->messageBus = b;
    session->socket = KINETIC_SOCKET_INVALID;  // start with an invalid file descriptor
    session->terminationStatus = KINETIC_STATUS_SUCCESS;
    return session;
}

void KineticAllocator_FreeSession(KineticSession* session)
{
    if (session != NULL) {
        KineticResourceWaiter_Destroy(&session->connectionReady);
        KineticFree(session);
    }
}

KineticResponse * KineticAllocator_NewKineticResponse(size_t const valueLength)
{
    KineticResponse * response = KineticCalloc(1, sizeof(*response) + valueLength);
    if (response == NULL) {
        LOG0("Failed allocating new response!");
        return NULL;
    }
    return response;
}

void KineticAllocator_FreeKineticResponse(KineticResponse * response)
{
    KINETIC_ASSERT(response != NULL);

    if (response->command != NULL) {
        protobuf_c_message_free_unpacked(&response->command->base, NULL);
    }
    if (response->proto != NULL) {
        protobuf_c_message_free_unpacked(&response->proto->base, NULL);
    }
    KineticFree(response);
}

KineticOperation* KineticAllocator_NewOperation(KineticSession* const session)
{
    KINETIC_ASSERT(session != NULL);

    LOGF3("Allocating new operation on session %p", (void*)session);
    KineticOperation* newOperation =
        (KineticOperation*)KineticCalloc(1, sizeof(KineticOperation));
    if (newOperation == NULL) {
        LOGF0("Failed allocating new operation on session %p", (void*)session);
        return NULL;
    }
    newOperation->session = session;
    newOperation->timeoutSeconds = session->timeoutSeconds; // TODO: use timeout in config throughput
    newOperation->request = (KineticRequest*)KineticCalloc(1, sizeof(KineticRequest));
    if (newOperation->request == NULL) {
        LOGF0("Failed allocating new PDU on session %p", (void*)session);
        KineticFree(newOperation);
        return NULL;
    }
    KineticRequest_Init(newOperation->request, session);
    return newOperation;
}

void KineticAllocator_FreeOperation(KineticOperation* operation)
{
    KINETIC_ASSERT(operation != NULL);
    LOGF3("Freeing operation %p on session %p", (void*)operation, (void*)operation->session);
    if (operation->request != NULL) {
        KineticFree(operation->request);
        operation->request = NULL;
    }
    if (operation->response != NULL) {
        KineticAllocator_FreeKineticResponse(operation->response);
        operation->response = NULL;
    }
    KineticFree(operation);
}

void KineticAllocator_FreeP2PProtobuf(Com__Seagate__Kinetic__Proto__Command__P2POperation* proto_p2pOp)
{
    if (proto_p2pOp != NULL) {
        if (proto_p2pOp->peer != NULL) {
            free(proto_p2pOp->peer);
            proto_p2pOp->peer = NULL;
        }
        if (proto_p2pOp->operation != NULL) {
            for(size_t i = 0; i < proto_p2pOp->n_operation; i++) {
                if (proto_p2pOp->operation[i] != NULL) {
                    if (proto_p2pOp->operation[i]->p2pop != NULL) {
                        KineticAllocator_FreeP2PProtobuf(proto_p2pOp->operation[i]->p2pop);
                        proto_p2pOp->operation[i]->p2pop = NULL;
                    }
                    if (proto_p2pOp->operation[i]->status != NULL) {
                        free(proto_p2pOp->operation[i]->status);
                        proto_p2pOp->operation[i]->status = NULL;
                    }
                    free(proto_p2pOp->operation[i]);
                    proto_p2pOp->operation[i] = NULL;
                }
            }
            free(proto_p2pOp->operation);
            proto_p2pOp->operation = NULL;
        }
        free(proto_p2pOp);
    }
}
