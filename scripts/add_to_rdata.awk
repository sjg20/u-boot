# SPDX-License-Identifier: GPL-2.0+
#
# Copyright 2023 Google, Inc
#
# Awk script to extract the default link script from the linker and write out
# the contents of INFILE immediately before the close of the .rdata section.

# to a C string which can be compiled into U-Boot.

# INS = 1 if we are inside the link script (delimited by ======== lines)
# INR = 1 if we are inside the .rdata section

# When we see } while in the .rdata part of the link script, insert INFILE
/}/ { if (INS && INR) { while ((getline < INFILE) > 0) {print}; DONE=1; INR=0; $0="}"; }}

# Find start and end of link script
/===================/ { if (INS) exit; INS=1; next; }

# If inside the link script, print each line
{ if (INS) print; }

# Detect the .rdata section and get ready to insert INFILE when we see the end }
/\.rdata.*:/ {INR=1; }

END { if (!DONE) { print "add_to_rdata.awk: Could not find link script in ld output" > "/dev/stderr"; exit 1;} }
