#!/bin/bash
set -e

echo "rebuilding..."
make clean

echo ""
echo "creating disk..."
sudo ./mkdisk.sh

echo ""
echo "building kernel..."
make all

echo ""
echo "run..."
echo "(serial output is available in serial.log)"
qemu-system-i386 \
  -boot d \
  -cdrom out/kOSeki.iso \
  -drive file=fat32.img,format=raw \
  -m 64 \
  -serial file:serial.log \
  -no-reboot