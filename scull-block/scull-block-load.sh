#!/bin/bash

module="scull-block"
device="scull-block"
minor=0
num_devices=2

exist=$(lsmod | grep "$module")

if [ -n "$exist" ]; then
    echo "[ERROR] module $module has already been loaded"
    exit 0
fi

echo "[INFO] loading module"
sudo insmod "$module.ko" $* || { echo "[ERROR] load module fail"; exit 1; }
major=$(grep "$module" /proc/devices | awk '{print $1}')

if [ -z "$major" ]; then
    echo "[ERROR] can't find module: $module in /proc/devices, removing loading module: $module"
    sudo rmmod "$module"
    exit 1
fi

find /dev -type c -name 'scull-block*' | xargs -I {} sudo rm {}

for ((i = 0; i < num_devices; i++)); do
    device_path="/dev/${device}${i}"
    sudo mknod "$device_path" c $major $((minor + i))
    sudo chgrp wheel "$device_path"
    sudo chmod 660 "$device_path"

    echo "[INFO] create device($major, $((minor+i))) $device_path"
done
