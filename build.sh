#!/usr/bin/env bash

echo
echo ================================================================================
echo Cleaning out old build artifacts...
echo ================================================================================
find src/**/*.o vendor/socket99/*.o vendor/protobuf-c/protobuf-c/*.o | xargs rm $1
rm libkinetic-c-client.a
rm src/utility/kinetic-c-util
echo DONE!

echo
echo ================================================================================
echo Building the kinetic-c lib sources and bundled dependencies
echo ================================================================================
cd src/lib/
gcc -Wall -c *.c -I. -I../../include/ -I../../vendor/
cd ../../
cd vendor/socket99/
gcc -Wall -std=c99 -c *.c
cd ../protobuf-c/protobuf-c
gcc -Wall -std=c99 -c *.c
cd ../../socket99/
gcc -Wall -std=c99 -c *.c
cd ../../
cd src/lib/
gcc -Wall -std=c99 -c *.c -I. -I../../include/ -I../../vendor/
cd ../..
echo DONE!

echo
echo ================================================================================
echo Assembling kinetic-c-client lib
echo ================================================================================
ar -cvq libkinetic-c-client.a src/lib/*.o vendor/protobuf-c/protobuf-c/*.o vendor/socket99/socket99.o
ar -t libkinetic-c-client.a
echo DONE!

echo
echo ================================================================================
echo Building kinetic-c-client utility
echo ================================================================================
cd src/utility/
gcc -o kinetic-c-util -std=c99 *.c ./examples/*.c  ../../libkinetic-c-client.a -I. -I../../include/ -I../lib -I ./examples/ -l crypto -l ssl
echo DONE!

echo
echo ================================================================================
echo Running kinetic-c-client examples
echo ================================================================================
./kinetic-c-util noop
cd ../../
echo DONE!
echo
echo Kinetic-C library and utility build complete!
