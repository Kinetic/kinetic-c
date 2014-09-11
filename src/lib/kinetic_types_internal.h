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

#ifndef _KINETIC_TYPES_INTERNAL_H
#define _KINETIC_TYPES_INTERNAL_H

#include "kinetic_types.h"
// #include <netinet/in.h>
// #include <ifaddrs.h>
// #include <openssl/sha.h>
// #ifndef _BSD_SOURCE
//     #define _BSD_SOURCE
// #endif // _BSD_SOURCE
// #include <unistd.h>
// #include <sys/types.h>
// #include "kinetic_proto.h"
// #include <time.h>

// // Ensure __func__ is defined (for debugging)
// #if __STDC_VERSION__ < 199901L
//     #if __GNUC__ >= 2
//         #define __func__ __FUNCTION__
//     #else
//         #define __func__ "<unknown>"
//     #endif
// #endif


// // // Structure for defining a custom memory allocator.
// // typedef struct 
// // {
// //     void        *(*alloc)(void *allocator_data, size_t size);
// //     void        (*free)(void *allocator_data, void *pointer);
// //     // Opaque pointer passed to `alloc` and `free` functions
// //     void        *allocator_data;
// // } ProtobufCAllocator;


// #define KINETIC_CONNECTION_INIT(_con, _id, _key) {
//     (*_con) = (KineticConnection) {
//         .socketDescriptor = -1,
//         .connectionID = time(NULL),
//         .identity = (_id),
//         .sequence = 0,
//     };
//     (*_con).key = (ByteArray){.data = (*_con).keyData, .len = (_key).len};
//     if ((_key).data != NULL && (_key).len > 0) {
//         memcpy((_con)->keyData, (_key).data, (_key).len); }
// }


// #define KINETIC_MESSAGE_HEADER_INIT(_hdr, _con) {
//     assert((void *)(_hdr) != NULL);
//     assert((void *)(_con) != NULL);
//     *(_hdr) = (KineticProto_Header) {
//         .base = PROTOBUF_C_MESSAGE_INIT(&KineticProto_header__descriptor),
//         .has_clusterVersion = true,
//         .clusterVersion = (_con)->clusterVersion,
//         .has_identity = true,
//         .identity = (_con)->identity,
//         .has_connectionID = true,
//         .connectionID = (_con)->connectionID,
//         .has_sequence = true,
//         .sequence = (_con)->sequence,
//     };
// }
// #define KINETIC_MESSAGE_INIT(msg) { 
//     KineticProto__init(&(msg)->proto); 
//     KineticProto_command__init(&(msg)->command); 
//     KineticProto_header__init(&(msg)->header); 
//     KineticProto_status__init(&(msg)->status); 
//     KineticProto_body__init(&(msg)->body); 
//     KineticProto_key_value__init(&(msg)->keyValue); 
//     memset((msg)->hmacData, 0, SHA_DIGEST_LENGTH); 
//     (msg)->proto.hmac.data = (msg)->hmacData; 
//     (msg)->proto.hmac.len = KINETIC_HMAC_MAX_LEN; 
//     (msg)->proto.has_hmac = true; 
//     (msg)->command.header = &(msg)->header; 
//     (msg)->proto.command = &(msg)->command; 
// }


// #define KINETIC_PDU_HEADER_INIT \
//     (KineticPDUHeader) {.versionPrefix = 'F'}


// #define KINETIC_PDU_INIT(_pdu, _con) {
//     assert((void *)(_pdu) != NULL);
//     assert((void *)(_con) != NULL);
//     (_pdu)->connection = (_con);
//     (_pdu)->header = KINETIC_PDU_HEADER_INIT;
//     (_pdu)->headerNBO = KINETIC_PDU_HEADER_INIT;
//     (_pdu)->value = BYTE_ARRAY_NONE;
//     (_pdu)->proto = &(_pdu)->protoData.message.proto;
//     KINETIC_MESSAGE_HEADER_INIT(&((_pdu)->protoData.message.header), (_con));
// }
// #define KINETIC_PDU_INIT_WITH_MESSAGE(_pdu, _con) {
//     KINETIC_PDU_INIT((_pdu), (_con))
//     KINETIC_MESSAGE_INIT(&((_pdu)->protoData.message));
//     (_pdu)->proto->command->header = &(_pdu)->protoData.message.header;
//     KINETIC_MESSAGE_HEADER_INIT(&(_pdu)->protoData.message.header, (_con));
// }


// #define KINETIC_OPERATION_INIT(_op, _con, _req, _resp)
// *(_op) = (KineticOperation) {
//     .connection = (_con),
//     .request = (_req),
//     .response = (_resp),
// }


#endif // _KINETIC_TYPES_INTERNAL_H
