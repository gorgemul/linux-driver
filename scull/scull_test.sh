#!/bin/bash

./scull_load.sh
if [ "$?" -ne "0" ]; then
    exit 1
fi
echo "[INFO] building test"
gcc -Wall -Wextra -pedantic -o scull_test scull_test.c
echo "[INFO] running test"
./scull_test
echo "[INFO] removing test"
rm scull_test
./scull_unload.sh
