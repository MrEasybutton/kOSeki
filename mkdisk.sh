#!/bin/bash
printf "CREATING DISK ⧖ "
dd if=/dev/zero of=fat32.img bs=1024 count=65536 > /dev/null 2>&1
mkfs.fat -F 32 fat32.img 
printf "Mounting...\n"
rm -rf /mnt/fat32
mkdir /mnt/fat32
mount fat32.img /mnt/fat32
printf "MOUNTED\n"
cp -r rootfs/* /mnt/fat32
umount fat32.img



