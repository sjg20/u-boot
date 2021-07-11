#!/bin/sh

# Script to build an EFI payload
# Just for example

TMP=/tmp/efi64
MNT=/mnt/x

qemu-img create try.img 20M
mkfs.vfat try.img
mkdir -p $TMP
cat >$TMP/startup.nsh <<EOF
fs0:u-boot-payload.efi
EOF
sudo cp /tmp/b/efi-x86_payload64/u-boot-payload.efi $TMP
sudo cp /boot/vmlinuz-5.4.0-77-generic $TMP/vmlinuz

sudo mount -o loop try.img $MNT
sudo cp $TMP/* $MNT
sudo umount $MNT
