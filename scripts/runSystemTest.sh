#! /usr/bin/env bash
BASE_DIR=`dirname "$0"`
BASE_DIR=`cd "$BASE_DIR"; pwd`

echo

if [[ -z $1 ]]; then
    echo Must specify system test to run!
    exit 1
fi

test=./bin/systest/run_$1
log=./bin/systest/$1.log
passfile=./bin/systest/$1.testpass

rm -f $passfile > /dev/null

echo ================================================================================
echo Running system test: $test
echo ================================================================================
$test | tee $log
cat $log | tail -n 1 | grep -e "OK" &> /dev/null
if [[ $? -ne 0 ]]; then
    exit 1
else
    touch $passfile
fi
echo ================================================================================
echo
