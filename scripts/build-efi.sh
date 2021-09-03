#!/bin/bash
# SPDX-License-Identifier: GPL-2.0+
#
# Script to build an EFI thing suitable for booting with qemu, possibly running
# it also.

# This just an example. It assumes that

# - you build U-Boot in /tmp/b/<name> where <name> is the U-Boot board config
# - /mnt/x is a directory used for mounting
# - you have access to the 'pure UEFI' builds for qemu
#
# UEFI binaries for QEMU used for testing this script:
#
# OVMF-pure-efi.i386.fd at
# https://drive.google.com/file/d/1jWzOAZfQqMmS2_dAK2G518GhIgj9r2RY/view?usp=sharing

# OVMF-pure-efi.x64.fd at
# https://drive.google.com/file/d/1c39YI9QtpByGQ4V0UNNQtGqttEzS-eFV/view?usp=sharing

set -e

usage() {
	echo "Usage: $0 [-a | -p]" 1>&2
	echo 1>&2
	echo "   -a   - Package up the app" 1>&2
	echo "   -o   - Use old EFI app build (before 32/64 split)" 1>&2
	echo "   -p   - Package up the payload" 1>&2
	echo "   -r   - Run qemu with the image" 1>&2
	echo "   -w   - Use word version (32-bit)" 1>&2
	exit 1
}

# 32- or 64-bit EFI
bitness=64

# app or payload ?
type=app

# run the image with qemu
run=

# before the 32/64 split of the app
old=

while getopts "aoprw" opt; do
	case "${opt}" in
	a)
		type=app
		;;
	p)
		type=payload
		;;
	r)
		run=1
		;;
	w)
		bitness=32
		;;
	o)
		old=1
		;;
	*)
		usage
		;;
	esac
done

run_qemu() {
	if [[ "${bitness}" = "64" ]]; then
		qemu=qemu-system-x86_64
		bios=OVMF-pure-efi.x64.fd
	else
		qemu=qemu-system-i386
		bios=OVMF-pure-efi.i386.fd
	fi
	echo "Running ${qemu}"
	"${qemu}" -bios "${bios}" \
		-drive id=disk,file=try.img,if=none,format=raw \
		-nic none -device ahci,id=ahci \
		-device ide-hd,drive=disk,bus=ahci.0
}

TMP="/tmp/efi${bitness}${type}"
MNT=/mnt/x
BUILD="efi-x86_${type}${bitness}"

if [[ -n "${old}" && "${bitness}" = "32" ]]; then
	BUILD="efi-x86_${type}"
fi

echo "Packaging ${BUILD}"
qemu-img create try.img 24M >/dev/null
mkfs.vfat try.img >/dev/null
mkdir -p $TMP
cat >$TMP/startup.nsh <<EOF
fs0:u-boot-${type}.efi
EOF
sudo cp /tmp/b/$BUILD/u-boot-${type}.efi $TMP

# Can copy in other files here:
#sudo cp /tmp/b/$BUILD/image.bin $TMP/chromeos.rom
#sudo cp /boot/vmlinuz-5.4.0-77-generic $TMP/vmlinuz

sudo mount -o loop try.img $MNT
sudo cp $TMP/* $MNT
sudo umount $MNT

if [[ -n "${run}" ]]; then
	run_qemu
fi
