#!/bin/sh

# Script to build an EFI payload
# Just for example

set -e

TMP=/tmp/efi32
MNT=/mnt/x
#BUILD=efi-x86_app32
BUILD=chromeos_efi-x86_app32
LOOP=/dev/loop30

mkdir -p $TMP
cat >$TMP/startup.nsh <<EOF
fs0:u-boot-app.efi
EOF
sudo cp /tmp/b/$BUILD/u-boot-app.efi $TMP
sudo cp /tmp/b/$BUILD/image.bin $TMP/chromeos.rom
#sudo cp /boot/vmlinuz-5.4.0-77-generic $TMP/vmlinuz

LOOP=$(losetup --show -P -f test_image.bin)
echo $LOOP

cleanup {
  losetup -d $LOOP
}

trap cleanup EXIT

sudo mount /dev/loop30p12 $MNT
sudo mkdir -p $MNT/efi/chromium
sudo cp -r $TMP/* $MNT/efi/chromium/.
sudo umount $MNT
