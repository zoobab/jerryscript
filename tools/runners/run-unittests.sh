#!/bin/bash

# Copyright 2014-2016 Samsung Electronics Co., Ltd.
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

DIR="$1"
shift

mkdir -p $DIR/check

UNITTEST_ERROR=$DIR/check/unittests.failed
UNITTEST_OK=$DIR/check/unittests.passed

rm -f $UNITTEST_ERROR $UNITTEST_OK

UNITTESTS=$(ls $DIR/unit-*)
total=$(ls $DIR/unit-* | wc -l)

if [ "$total" -eq 0 ]
then
    echo "$0: $DIR: no unit-* test to execute"
    exit 1
fi

tested=1
failed=0
passed=0

UNITTEST_TEMP=`mktemp unittest-out.XXXXXXXXXX`

for unit_test in $UNITTESTS
do
  echo -n "[$tested/$total] $unit_test: "

  $unit_test &>$UNITTEST_TEMP
  status_code=$?

    if [ $status_code -ne 0 ]
    then
        echo "FAIL ($status_code)"
        cat $UNITTEST_TEMP

        echo "$status_code: $unit_test" >> $UNITTEST_ERROR
        echo "============================================" >> $UNITTEST_ERROR
        cat $UNITTEST_TEMP >> $UNITTEST_ERROR
        echo "============================================" >> $UNITTEST_ERROR
        echo >> $UNITTEST_ERROR
        echo >> $UNITTEST_ERROR

        failed=$((failed+1))
    else
        echo "PASS"

        echo "$unit_test" >> $UNITTEST_OK

        passed=$((passed+1))
    fi

    tested=$((tested+1))
done

rm -f $UNITTEST_TEMP

ratio=$(echo $passed*100/$total | bc)

echo "[summary] $DIR/unit-*: $passed PASS, $failed FAIL, $total total, $ratio% success"

if [ $failed -ne 0 ]
then
    echo "$0: see $UNITTEST_ERROR for details about failures"
    exit 1
fi

exit 0
