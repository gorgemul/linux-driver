#!/bin/bash

module="scull"
device="scull"

echo "[INFO] removing devices /dev/${device}*"
find /dev -type c -name 'scull*' | xargs -I {} sudo rm {}

echo "[INFO] remove module $module"
sudo rmmod $module || echo "[ERROR] remove module fail"
