#!/bin/sh
# Run astyle on all C files in all project source folders
astyle --style=kr --break-closing-brackets --pad-oper --pad-header --unpad-paren --convert-tabs --indent=spaces=4 --lineend=linux --recursive src/*.* ./include/*.h
