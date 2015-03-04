/*
 * kinetic-c-client
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
#ifndef KINETIC_ACL_H
#define KINETIC_ACL_H

#include "acl_types.h"
#include "kinetic_proto.h"

/* Attempt to instantiate an ACL structure based on the JSON data
 * contained in PATH. */
KineticACLLoadResult
KineticACL_LoadFromFile(const char *path, struct ACL **instance);

KineticACLLoadResult
acl_of_string(const char *buf, size_t buf_size, struct ACL **instance);

/* Print an ACL struct to the specified file. */
void KineticACL_Print(FILE *f, struct ACL *acl);

/* Free an ACL struct */
void KineticACL_Free(struct ACL *acl);

#endif // KINETIC_ACL_H
