/*
* kinetic-c
* Copyright (C) 2015 Seagate Technology.
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
#include "protobuf-c/protobuf-c.h"
#include "byte_array.h"
#include "kinetic_types.h"
#include "kinetic.pb-c.h"
#include "kinetic_logger.h"
#include "kinetic_types_internal.h"
#include "mock_kinetic_operation.h"
#include "mock_kinetic_controller.h"
#include "mock_kinetic_session.h"
#include "mock_kinetic_message.h"
#include "mock_kinetic_bus.h"
#include "mock_kinetic_response.h"
#include "mock_kinetic_device_info.h"
#include "mock_kinetic_allocator.h"
#include "mock_kinetic_request.h"
#include "mock_kinetic_acl.h"
#include "kinetic_callbacks.h"

void test_kinetic_callbacks_needs_testing(void)
{
    TEST_IGNORE_MESSAGE("TODO: Test operation callbacks.");
}

// void test_KineticBuilder_GetLogCallback_should_copy_returned_device_info_into_dynamically_allocated_info_structure(void)
// {
//     // KineticRequest response;
//     // KineticLogInfo* info;
//     // KineticOperation op = {
//     //     .response = &response,
//     //     .deviceInfo = &info,
//     // };

//     TEST_IGNORE_MESSAGE("TODO: Need to implement!")

//     // KineticSerialAllocator_Create()

//     // KineticStatus status = KineticOperation_GetLogCallback(&op);
// }
