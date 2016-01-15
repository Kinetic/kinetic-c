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
