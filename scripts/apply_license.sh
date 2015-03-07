#!/bin/sh

# Get your array of filenames
SOURCES=`find include/* -name *.h`
SOURCES="$SOURCES `find src/**/* -name *.[ch]`"
SOURCES="$SOURCES `find test/**/* -name *.[ch]`"

for file in ${SOURCES[*]/\.\//}; do
    if grep -q "Copyright (C) 2015 Seagate Technology." $file; then
        echo Skipping ${file}
    else
        echo Applying license to ${file}...
        cat ./config/license_template $file > tmp && mv tmp $file
    fi
done
