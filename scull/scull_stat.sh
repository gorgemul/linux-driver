#!/bin/bash

echo "[STAT] /proc/devices"
cat /proc/devices  | grep scull || { echo "NULL"; }
echo "[STAT] lsmod"
lsmod | grep scull || { echo "NULL"; }
echo "[STAT] /dev/scull*"
ls -alh /dev | grep scull || { echo "NULL"; }
