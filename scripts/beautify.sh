#!/bin/sh
# Run astyle on all C files in all project source folders
astyle --suffix=none --style=kr --break-closing-brackets --pad-oper --pad-header --unpad-paren --convert-tabs --indent=spaces=4 --indent-col1-comments --min-conditional-indent=0 --align-pointer=type --align-reference=type --keep-one-line-statements --lineend=linux --recursive "src/*.h" --recursive "src/*.c" --recursive "test/*.c" --recursive "test/*.h"  --recursive "include/*.h"
