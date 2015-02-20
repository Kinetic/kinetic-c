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
#ifndef ACL_H
#define ACL_H

#include "acl_types.h"

/* Attempt to instantiate an ACL structure based on the JSON data
 * contained in PATH. */
acl_of_file_res
acl_of_file(const char *path, struct ACL **instance);

/* fprintf an ACL struct. */
void acl_fprintf(FILE *f, struct ACL *acl);

/* Free an ACL struct */
void acl_free(struct ACL *acl);

#endif
