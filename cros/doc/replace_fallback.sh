#!/bin/sh

# SPDX-License-Identifier: GPL-2.0
# Copyright 2021 Google LLC

# Replace fallback/payload with U-Boot in the image
# This uses cbfstool which prints a long usage message on any error. Look at the
# top for the error message.

# TODO: Consider using binman to do this. At present it does not support
# inferring an fdtmap from a Chromium OS image.

# Use this one if you don't want output from coreboot
old=~/image-coral.bin

# Use this one if you do
#old=~/image-coral.dev.bin

# Location of developer keys, for signing
devkeys_dir=cros/data/devkeys

# Output filename
new=cb.bin

# Input directory
ub=/tmp/b/chromeos_coral

# Name of file to replace
name=fallback/payload

# Run cbfstool and die if something goes wrong
do_cbfstool() {
	local output
	output=$(cbfstool "$@" 2>&1)
	if [ $? != 0 ]; then
		echo here
		die "Failed cbfstool invocation: cbfstool $@\n${output}"
	fi
	printf "${output}"
}

# Delete various locales to create plenty of space for U-Boot, etc.
# This is not strictly necessary, but we cannot know where $name appears in
# the image, so even a size increase of 20 bytes will not be supported, since
# cbfstool does not support moving existing files.
# Args:
#   $1: Filename of image to update
delete_locales() {
	local rom="$1"

	for f in locale_tr.bin \
		locale_bg.bin \
		locale_hr.bin \
		locale_nl.bin \
		locale_ro.bin \
		locale_es.bin \
		locale_ja.bin \
		locale_el.bin \
		locale_pt-BR.bin \
		locale_de.bin \
		locale_lt.bin \
		locale_hi.bin \
		locale_vi.bin \
		locale_ca.bin \
		locale_hu.bin \
		locale_sr.bin \
		locale_pt-PT.bin \
		locale_fi.bin \
		locale_cs.bin \
		locale_he.bin \
		locale_fr.bin \
		locale_id.bin \
		locale_ta.bin \
		locale_bn.bin \
		locale_th.bin \
		locale_ar.bin \
		locale_it.bin \
		locale_pl.bin \
		locale_lv.bin \
		locale_mr.bin \
		locale_es-419.bin \
		locale_et.bin \
		locale_ms.bin \
		locale_nb.bin \
		locale_zh-CN.bin \
		locale_ko.bin \
		locale_fil.bin \
		locale_da.bin \
		locale_ru.bin \
		locale_uk.bin \
		locale_sl.bin \
		locale_te.bin \
		locale_sk.bin \
		locale_gu.bin \
		locale_sv.bin \
		locale_kn.bin \
		locale_ml.bin \
		locale_fa.bin \
		locale_zh-TW.bin \
		vbt-astronaut.bin vbt-epaulette.bin vbt-babytiger.bin \
		vbt-babymega.bin vbt-nasher.bin vbt-rabbid_rugged.bin; do
		do_cbfstool $rom remove -n $f
	done
}

# Process one region of the image
# Removes unwanted files and adds in the u-boot.bin binary
# Args:
#  $1: Filename of image to update
#  $2: region params ('' for recovery, '-r FW_MAIN_A' for firmare a)
do_part() {
	local fw_image="$1"
	local region="$2"

	do_cbfstool $fw_image remove $region -n $name

	# Drop the unused bootblock in recovery to add space
	if [ -z "$region" ]; then
		 do_cbfstool $fw_image remove $region -n bootblock
		delete_locales $fw_image
	fi

	do_cbfstool $fw_image expand $region

	if ! do_cbfstool $fw_image add-flat-binary $region -n $name \
		-f $ub/u-boot.bin -c lzma -l 0x1110000 -e 0x1110000; then
		do_cbfstool $fw_image print $region
	fi
}

# Size the image and update signature region (VBLOCK_A/B)
# Args:
#  $1: Filename of image to update
#  $2: Directory containing (dev) keys
#  $3: Slot to update (either "A" or "B")
sign_region() {
	local fw_image="$1"
	local keydir="$2"
	local slot="$3"

	local tmpfile=`mktemp`
	local cbfs=FW_MAIN_${slot}
	local vblock=VBLOCK_${slot}

	do_cbfstool ${fw_image} read -r ${cbfs} -f ${tmpfile}

	# Read the contents of $cbfs and get the last line, e.g.:
	# (empty)	0x121c80	null	0x28	0x34e314	0x34e33c
	# Then use sed to get its offset in the file (e.g. 0x121c80)
	local size=$(do_cbfstool ${fw_image} print -k -r ${cbfs} | \
		tail -1 | \
		sed "/(empty).*null/ s,^(empty)[[:space:]]\(0x[0-9a-f]*\)\tnull\t.*$,\1,")
	size=$(printf "%d" ${size})

	# If the last entry is called "(empty)" and of type "null", remove it
	# from the section so it isn't part of the signed data, to improve boot
	# speed, if (as often happens) there's a large unused suffix.
	if [ -n "${size}" ] && [ ${size} -gt 0 ]; then
		head -c ${size} ${tmpfile} > ${tmpfile}.2
		mv ${tmpfile}.2 ${tmpfile}
		# Use 255 (aka 0xff) as the filler, this greatly reduces
		# memory areas which need to be programmed for spi flash
		# chips, because the erase value is 0xff.
		do_cbfstool ${fw_image} write --force -u -i 255 \
			-r ${cbfs} -f ${tmpfile}
	fi

	futility vbutil_firmware \
		--vblock ${tmpfile}.out \
		--keyblock ${keydir}/firmware.keyblock \
		--signprivate ${keydir}/firmware_data_key.vbprivk \
		--version 1 \
		--fv ${tmpfile} \
		--kernelkey ${keydir}/kernel_subkey.vbpubk \
		--flags 0

	# Write the signature to the vblock region
	do_cbfstool ${fw_image} write -u -i 255 -r ${vblock} -f ${tmpfile}.out

	rm -f ${tmpfile} ${tmpfile}.out
}

# Add signatures for the updated image, to both A and B
# Args:
#  $1: Filename of image to update
#  $2: Directory containing (dev) keys
sign_image() {
	local fw_image=$1
	local keydir=$2

	sign_region "${fw_image}" "${keydir}" A
	sign_region "${fw_image}" "${keydir}" B
}

# Add the Memory-Reference-Code (MRC) cache data to speed up the first boot
# This saves about 21 seconds on coral`
# Args:
#  $1: Filename of image to update
add_mrc() {
	local fw_image=$1

	# These files can be created from the image with:
	# dump_fmap rom.bin -x RECOVERY_MRC_CACHE RW_MRC_CACHE RW_VAR_MRC_CACHE
	do_cbfstool $fw_image write -r RW_MRC_CACHE -f RW_MRC_CACHE
	do_cbfstool $fw_image write -r RW_VAR_MRC_CACHE -f RW_VAR_MRC_CACHE
	do_cbfstool $fw_image write -r RECOVERY_MRC_CACHE -f RECOVERY_MRC_CACHE
}

# Start with the original image
cp $old $new

# Do read/write and recovery
do_part $new "-r FW_MAIN_A"
do_part $new "-r FW_MAIN_B"
do_part $new

# Sign the image with dev keys for A and B, so it will boot
sign_image $new "${devkeys_dir}"

# Optionally, put valid MRC-cache data in there for a faster boot
# add_mrc
if [ -f RW_MRC_CACHE ]; then
	add_mrc $new
fi
