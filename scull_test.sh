#!/bin/bash

echo "[INFO] loading module"
./scull_load.sh
if [ "$?" -ne "0" ]; then
    echo "[INFO] fail to load module"
    exit 1
fi
echo "[INFO] start running test"
./scull_test.py
echo "[INFO] unloading module"
./scull_unload.sh
