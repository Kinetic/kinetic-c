#!/bin/sh
# Run astyle on all C files in all project source folders
astyle --style=allman --break-blocks=all --pad-oper --pad-header --unpad-paren --align-pointer=type --add-brackets --convert-tabs --lineend=linux --recursive ./src/*.c ./src/*.h ./include/*.h
