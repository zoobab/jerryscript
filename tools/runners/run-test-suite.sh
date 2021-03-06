#!/bin/bash

# Copyright 2015-2016 Samsung Electronics Co., Ltd.
# Copyright 2016 University of Szeged
#
# Licensed under the Apache License, Version 2.0 (the "License");
# you may not use this file except in compliance with the License.
# You may obtain a copy of the License at
#
#     http://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing, software
# distributed under the License is distributed on an "AS IS" BASIS
# WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
# See the License for the specific language governing permissions and
# limitations under the License.

TIMEOUT=${TIMEOUT:=5}

ENGINE="$1"
shift

OUT_DIR="$1"
shift

TESTS="$1"
shift

ENGINE_ARGS="$@"

if [ ! -x $ENGINE ]
then
    echo "$0: $ENGINE: not an executable"
    exit 1
fi

mkdir -p $OUT_DIR

TEST_FILES=$OUT_DIR/test.files
TEST_FAILED=$OUT_DIR/test.failed
TEST_PASSED=$OUT_DIR/test.passed

if [ -d $TESTS ]
then
    TESTS_DIR=$TESTS

    ( cd $TESTS; find . -path fail -prune -o -name "[^N]*.js" -print ) | sort > $TEST_FILES

    if [ -d $TESTS/fail ]
    then
        for error_code in `cd $TESTS/fail && ls -d [0-9]*`
        do
            ( cd $TESTS; find ./fail/$error_code -name "[^N]*.js" -print ) | sort >> $TEST_FILES
        done
    fi
elif [ -f $TESTS ]
then
    TESTS_DIR=`dirname $TESTS`

    cp $TESTS $TEST_FILES
else
    echo "$0: $TESTS: not a test suite"
    exit 1
fi

total=$(cat $TEST_FILES | wc -l)
if [ "$total" -eq 0 ]
then
    echo "$0: $TESTS: no test in test suite"
    exit 1
fi

rm -f $TEST_FAILED $TEST_PASSED

tested=1
failed=0
passed=0

ENGINE_TEMP=`mktemp engine-out.XXXXXXXXXX`

for test in `cat $TEST_FILES`
do
    error_code=`echo $test | grep -e "^.\/fail\/[0-9]*\/" -o | cut -d / -f 3`
    if [ "$error_code" = "" ]
    then
        PASS="PASS"
        error_code=0
    else
        PASS="PASS (XFAIL)"
    fi

    full_test=$TESTS_DIR/${test#./}

    echo -n "[$tested/$total] $ENGINE $ENGINE_ARGS $full_test: "

    ( ulimit -t $TIMEOUT; $ENGINE $ENGINE_ARGS $full_test &>$ENGINE_TEMP; exit $? )
    status_code=$?

    if [ $status_code -ne $error_code ]
    then
        echo "FAIL ($status_code)"
        cat $ENGINE_TEMP

        echo "$status_code: $test" >> $TEST_FAILED
        echo "============================================" >> $TEST_FAILED
        cat $ENGINE_TEMP >> $TEST_FAILED
        echo "============================================" >> $TEST_FAILED
        echo >> $TEST_FAILED
        echo >> $TEST_FAILED

        failed=$((failed+1))
    else
        echo "$PASS"

        echo "$test" >> $TEST_PASSED

        passed=$((passed+1))
    fi

    tested=$((tested+1))
done

rm -f $ENGINE_TEMP

ratio=$(echo $passed*100/$total | bc)

echo "[summary] $ENGINE $ENGINE_ARGS $TESTS: $passed PASS, $failed FAIL, $total total, $ratio% success"

if [ $failed -ne 0 ]
then
    echo "$0: see $TEST_FAILED for details about failures"
    exit 1
fi

exit 0
