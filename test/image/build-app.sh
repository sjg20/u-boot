#!/bin/sh

# Script to build an EFI payload
# Just for example

TMP=/tmp/efi32
MNT=/mnt/x
#BUILD=efi-x86_app32
BUILD=chromeos_efi-x86_app32

qemu-img create try.img 20M
mkfs.vfat try.img
mkdir -p $TMP
cat >$TMP/startup.nsh <<EOF
fs0:u-boot-app.efi
EOF
sudo cp /tmp/b/$BUILD/u-boot-app.efi $TMP
sudo cp /boot/vmlinuz-5.4.0-77-generic $TMP/vmlinuz

sudo mount -o loop try.img $MNT
sudo cp $TMP/* $MNT
sudo umount $MNT
