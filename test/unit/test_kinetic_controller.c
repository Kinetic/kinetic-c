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

#include "unity_helper.h"
#include "byte_array.h"
#include "kinetic_types.h"
#include "kinetic_types_internal.h"
#include "kinetic_controller.h"
#include "kinetic_logger.h"
#include "kinetic_proto.h"
#include "protobuf-c.h"
#include "mock_kinetic_connection.h"
#include "mock_kinetic_socket.h"
#include "mock_kinetic_operation.h"
#include "mock_kinetic_pdu.h"
#include "mock_kinetic_allocator.h"
#include <pthread.h>


// static KineticConnection Connection;
// static int64_t ConnectionID = 12345;
// static KineticPDU Request, Response;
// static KineticPDU Requests[3];
// static KineticOperation Operation;

void setUp(void)
{
    KineticLogger_Init("stdout", 3);
    // KINETIC_CONNECTION_INIT(&Connection);
    // Connection.connectionID = ConnectionID;
    // KINETIC_PDU_INIT_WITH_COMMAND(&Request, &Connection);
    // KINETIC_PDU_INIT_WITH_COMMAND(&Response, &Connection);
    // KINETIC_OPERATION_INIT(&Operation, &Connection);
    // Operation.request = &Request;
}

void tearDown(void)
{
    KineticLogger_Close();
}

// void test_KineticController_Init_should_create_and_kickoff_worker_threads(void)
// {
//     TEST_IGNORE_MESSAGE("TODO: Add unit tests for KineticController_Init");
// }

// void test_KineticController_CreateOperation_should_create_a_new_operation_for_the_specified_session(void)
// {
//     TEST_IGNORE_MESSAGE("TODO: Add unit tests for KineticController_CreateOperation");
// }

// void test_KineticController_ExecuteOperation_should_execute_the_specified_operation(void)
// {
//     TEST_IGNORE_MESSAGE("TODO: Add unit tests for KineticController_ExecuteOperation");
// }

// void test_KineticController_Pause_should_pause_worker_threads_when_paused(void)
// {
//     TEST_IGNORE_MESSAGE("TODO: Add unit tests for KineticController_Pause");
// }

// void test_KineticController_ReceiveThread_should_service_response_PDUs(void)
// {
//     TEST_IGNORE_MESSAGE("TODO: Add unit tests for KineticController_ReceiveThread");
// }
