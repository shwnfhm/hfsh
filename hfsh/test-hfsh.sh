#! /bin/bash

if ! [[ -x hfsh ]]; then
    echo "hfsh executable does not exist"
    exit 1
fi

../tester/run-tests.sh $*


