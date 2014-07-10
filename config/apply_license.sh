#!/bin/sh

for file in "$@"
do
    if grep -q "Copyright (C) 2014 Seagate Technology." $file; then
        echo Skipping ${file}
    else
        echo Applying license to ${file}...
        cat ./config/license_template $file > tmp && mv tmp $file
    fi
done
