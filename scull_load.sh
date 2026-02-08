#!/bin/bash

module="scull"
device="scull"
minor=0
num_devices=3

if [ "$num_devices" -le 0 ]; then
    echo "[ERROR] num_devices must be greater than 0"
    exit 1
fi

echo "[INFO] loading module $module"
sudo insmod "$module.ko" $* || { echo "[ERROR] load module fail"; exit 1; }
major=$(grep "$module" /proc/devices | awk '{print $1}')

echo "[INFO] removing devices /dev/${device}*"
find /dev -type c -name 'scull*' | xargs -I {} sudo rm {}

if [ -z "$major" ]; then
    echo "[ERROR] can't find module: $module in /proc/devices, removing the module..."
    sudo rmmod "$module"
    exit 1
fi

for ((i = 0; i < num_devices; i++)); do
    device_path="/dev/${device}${i}"
    sudo mknod "$device_path" c $major $((minor + i))
    sudo chgrp wheel "$device_path"
    sudo chmod 660 "$device_path"

    echo "[INFO] create device($major, $((minor+i))) $device_path"
done
