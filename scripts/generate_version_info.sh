#!/bin/sh
#generate kinetic_version_info.h header file

#the c file generated kinetic.proto using protoc
PROTO_SOURCE="src/lib/kinetic.pb-c.c"

#the header file waiting for generating
INFO_HEADER_FILE="src/lib/kinetic_version_info.h"

KINETIC_C_VERSION=`cat config/VERSION | sed 's/^ *\(.*\) *$/\1/'`
COMMIT_HASH=`git rev-parse HEAD | sed 's/^ *\(.*\) *$/\1/'`
PROTOCOL_VERSION=`grep "com__seagate__kinetic__proto__local__protocol_version__default_value\[\]" "$PROTO_SOURCE" | awk -F "\"" '{print $2}'`

#generate kinetic version header file
echo "generating $INFO_HEADER_FILE..."

touch "$INFO_HEADER_FILE"
echo "#ifndef _KINETIC_VERSION_INFO_H" > "$INFO_HEADER_FILE"
echo "#define _KINETIC_VERSION_INFO_H" >> "$INFO_HEADER_FILE"
echo "#define KINETIC_C_VERSION \"$KINETIC_C_VERSION\"" >> "$INFO_HEADER_FILE"
echo "#define KINETIC_C_PROTOCOL_VERSION \"$PROTOCOL_VERSION\"" >>"$INFO_HEADER_FILE"
echo "#define KINETIC_C_REPO_HASH \"$COMMIT_HASH\"" >> "$INFO_HEADER_FILE"
echo "#endif" >> "$INFO_HEADER_FILE"