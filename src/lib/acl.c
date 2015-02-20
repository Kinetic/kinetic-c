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
#include <string.h>
#include <stdio.h>

#include "acl.h"
#include "json.h"

typedef struct {
    permission_t permission;
    const char *string;
} permission_pair;

static permission_pair permission_table[] = {
    { PERM_INVALID, "INVALID" },
    { PERM_READ, "READ" },
    { PERM_WRITE, "WRITE" },
    { PERM_DELETE, "DELETE" },
    { PERM_RANGE, "RANGE" },
    { PERM_SETUP, "SETUP" },
    { PERM_P2POP, "P2POP" },
    { PERM_GETLOG, "GETLOG" },
    { PERM_SECURITY, "SECURITY" },
};

#define PERM_TABLE_ROWS sizeof(permission_table)/sizeof(permission_table)[0]

static acl_of_file_res unpack_scopes(struct ACL *acl,
    int scope_count, json_object *scopes);

static const char *str_of_permission(permission_t perm) {
    for (size_t i = 0; i < PERM_TABLE_ROWS; i++) {
        permission_pair *pp = &permission_table[i];
        if (pp->permission == perm) { return pp->string; }
    }
    return "INVALID";
}

static permission_t permission_of_str(const char *str) {
    for (size_t i = 0; i < PERM_TABLE_ROWS; i++) {
        permission_pair *pp = &permission_table[i];
        if (0 == strcmp(str, pp->string)) { return pp->permission; }
    }
    return PERM_INVALID;
}

acl_of_file_res
acl_of_file(const char *path, struct ACL **instance) {
    if (path == NULL || instance == NULL) {
        return ACL_ERROR_NULL;
    }
    
    struct json_object *obj = json_object_from_file(path);
    if (obj == NULL) { return ACL_ERROR_BAD_JSON; }
    
    acl_of_file_res res = ACL_ERROR_NULL;
    
    int scope_count = 0;
    struct json_object *val = NULL;
    if (json_object_object_get_ex(obj, "scope", &val)) {
        scope_count = json_object_array_length(val);
    } else {
        res = ACL_ERROR_MISSING_FIELD;
        goto cleanup;
    }
    
    struct ACL *acl = NULL;
    size_t alloc_sz = sizeof(*acl) + scope_count * sizeof(struct acl_scope);
    acl = malloc(alloc_sz);
    if (acl == NULL) {
        res = ACL_ERROR_MEMORY;
        goto cleanup;
    }

    memset(acl, 0, alloc_sz);
    
    /* Copy fields */
    if (json_object_object_get_ex(obj, "identity", &val)) {
        acl->identity = json_object_get_int64(val);
    } else {
        acl->identity = ACL_NO_IDENTITY;
    }
    
    if (json_object_object_get_ex(obj, "key", &val)) {
        const char *key = json_object_get_string(val);
        size_t len = strlen(key);
        struct hmac_key *hmacKey = malloc(sizeof(*hmacKey) + len + 1);
        if (hmacKey) {
            hmacKey->type = HMAC_SHA1;
            hmacKey->length = len;
            memcpy(hmacKey->key, key, len);
            hmacKey->key[len] = '\0';
            acl->hmacKey = hmacKey;
        } else {
            res = ACL_ERROR_MEMORY;
            goto cleanup;
        }
    }
    
    if (json_object_object_get_ex(obj, "HMACAlgorithm", &val)) {
        const char *hmac = json_object_get_string(val);
        if (0 != strcmp(hmac, "HmacSHA1")) {
            res = ACL_ERROR_INVALID_FIELD;
            goto cleanup;
        } else {
            // acl->key->type is already set to HMAC_SHA1.
        }
    }
    
    struct json_object *scopes = NULL;
    if (json_object_object_get_ex(obj, "scope", &scopes)) {
        res = unpack_scopes(acl, scope_count, scopes);
        if (res != ACL_OK) { goto cleanup; }
    }
    
    /* Decrement JSON object refcount, freeing it. */
    json_object_put(obj);
    *instance = acl;
    return ACL_OK;
    
cleanup:
    if (obj) { json_object_put(obj); }
    return res;
}

static acl_of_file_res unpack_scopes(struct ACL *acl, int scope_count, json_object *scopes) {
    for (int si = 0; si < scope_count; si++) {
        struct json_object *cur_scope = json_object_array_get_idx(scopes, si);
        if (cur_scope) {
            struct acl_scope *scope = &acl->scopes[si];
            for (int i = 0; i < ACL_MAX_PERMISSIONS; i++) {
                scope->permissions[i] = PERM_INVALID;
            }
            struct json_object *val = NULL;
            if (json_object_object_get_ex(cur_scope, "offset", &val)) {
                scope->offset = json_object_get_int64(val);
            } else {
                scope->offset = ACL_NO_OFFSET;
            }
            if (json_object_object_get_ex(cur_scope, "value", &val)) {
                const char *str = json_object_get_string(val);
                if (str) {
                    size_t len = strlen(str);
                    scope->value = malloc(len + 1);
                    if (scope->value == NULL) {
                        return ACL_ERROR_MEMORY;
                    } else {
                        memcpy(scope->value, str, len);
                        scope->value[len] = '\0';
                    }
                    scope->valueSize = len;
                }
            }
            scope->permission_count = 0;
            if (json_object_object_get_ex(cur_scope, "permission", &val)) {
                enum json_type perm_type = json_object_get_type(val);
                if (perm_type == json_type_string) {
                    scope->permissions[0] = permission_of_str(json_object_get_string(val));
                    if (scope->permissions[0] == PERM_INVALID) {
                        return ACL_ERROR_INVALID_FIELD;
                    } else {
                        scope->permission_count++;
                    }
                } else if (perm_type == json_type_array) {
                    int count = json_object_array_length(val);
                    for (int i = 0; i < count; i++) {
                        struct json_object *jperm = json_object_array_get_idx(val, i);
                        permission_t p = permission_of_str(json_object_get_string(jperm));
                        if (p == PERM_INVALID) {
                            return ACL_ERROR_INVALID_FIELD;
                        } else {
                            scope->permissions[scope->permission_count] = p;
                            scope->permission_count++;
                        }
                    }
                } else {
                    return ACL_ERROR_INVALID_FIELD;
                }
            }
            if (json_object_object_get_ex(cur_scope, "TlsRequired", &val)) {
                scope->tlsRequired = json_object_get_boolean(val);
            }
        }
    }
    acl->scopeCount = scope_count;
    return ACL_OK;
}

void acl_fprintf(FILE *f, struct ACL *acl) {
    if (acl == NULL) {
        fprintf(f, "NULL\n");
    } else {
        fprintf(f, "ACL:\n");
        if (acl->identity != ACL_NO_IDENTITY) {
            fprintf(f, "  identity: %lld\n", acl->identity);
        }
        if (acl->hmacKey) {
            fprintf(f, "  key[%s,%zd]: \"%s\"\n",
                "HmacSHA1", acl->hmacKey->length, acl->hmacKey->key);
        }
        fprintf(f, "  scopes: (%zd)\n", acl->scopeCount);
        
        for (size_t i = 0; i < acl->scopeCount; i++) {
            struct acl_scope *scope = &acl->scopes[i];
            fprintf(f, "    scope %zd:\n", i);
            if (scope->offset != ACL_NO_OFFSET) {
                fprintf(f, "      offset: %lld\n", scope->offset);
            }
            if (scope->value) {
                fprintf(f, "      value[%zd]: \"%s\"\n",
                    scope->valueSize, scope->value);
            }
            for (size_t pi = 0; pi < scope->permission_count; pi++) {
                if (scope->permissions[pi] != PERM_INVALID) {
                    fprintf(f, "      permission: %s\n",
                        str_of_permission(scope->permissions[pi]));
                }
            }
            fprintf(f, "      tlsRequired: %d\n", scope->tlsRequired);
        }
    }
}

void acl_free(struct ACL *acl) {
    if (acl) {
        if (acl->hmacKey) { free(acl->hmacKey); }
        for (size_t i = 0; i < acl->scopeCount; i++) {
            if (acl->scopes[i].value) { free(acl->scopes[i].value); }
        }
        free(acl);
    }
}
