#!/bin/bash -e

# Script to build an EFI payload
# Just for example

#TEST_IMAGE=test_image.bin
TEST_IMAGE=volteer_test_image.bin

TMP=/tmp/efi32
MNT=/mnt/x
#BUILD=efi-x86_app32
BUILD=chromeos_efi-x86_app32

mkdir -p $TMP
cat >$TMP/startup.nsh <<EOF
fs0:efi\chromium\u-boot-app.efi
EOF
sudo cp /tmp/b/$BUILD/u-boot-app.efi $TMP
sudo cp /tmp/b/$BUILD/image.bin $TMP/chromeos.rom
#sudo cp /boot/vmlinuz-5.4.0-77-generic $TMP/vmlinuz

LOOP=$(sudo losetup --show -P -f $TEST_IMAGE)
echo $LOOP

function cleanup {
  sudo umount $MNT
  sudo losetup -d $LOOP
  echo cleaned up $LOOP
}

trap cleanup EXIT

sleep 1
sudo mount ${LOOP}p12 $MNT

sudo rm -rf $MNT/efi/boot
sudo rm -f $MNT/syslinux/vmlinuz.*
sudo rm -f $MNT/efi/chromium/vmlinuz

sudo mkdir -p $MNT/efi/chromium
ls $MNT
sudo cp -r $TMP/* $MNT/efi/chromium/.
sudo cp -r $TMP/startup.nsh $MNT/
df -h $MNT
ls -lR $MNT
