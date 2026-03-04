#!/bin/bash

module="scull-block"
device="scull-block"

echo "[INFO] removing devices /dev/${device}*"
find /dev -type c -name 'scull-block*' | xargs -I {} sudo rm {}

echo "[INFO] removing module $module"
sudo rmmod $module || echo "[ERROR] rmmod module fail"
