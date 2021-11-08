// SPDX-License-Identifier: GPL-2.0+
/*
 * Copyright 2021 Google LLC
 * Written by Simon Glass <sjg@chromium.org>
 */

#include "mkimage.h"
#include "fit_common.h"
#include <image.h>

void usage(char *cmdname)
{
	fprintf(stderr,
		"Usage: %s -f dtb_file file -k key file [-o out_file]\n"
		"          -f ==> set dtb file which should be signed\n"
		"          -G ==> set signing key to use\n"
		"          -k ==> set directory containing private keys\n"
		"          -K ==> set DTB file to receive signing key\n"
		"          -o ==> if not provided, dtb file is updated\n"
		"          -S ==> name to use for signaure (defaults to -G)",
		cmdname);
	exit(EXIT_FAILURE);
}

static int sign_fdt(struct image_tool_params *params, size_t size_inc,
		    char *blob, int size, const char *outfile)
{
	void *dest_blob = NULL;
	int destfd = 0;
	int ret;

	if (outfile) {
		char *new_blob = malloc(size);

		if (!new_blob) {
			fprintf(stderr, "Out of memory\n");
			return -ENOMEM;
		}
		memcpy(new_blob, blob, size);
		blob = new_blob;
	}

	if (params->keydest) {
		struct stat dest_sbuf;

		destfd = mmap_fdt(params->cmdname, params->keydest, size_inc,
				  &dest_blob, &dest_sbuf, false,
				  false);
		if (destfd < 0) {
			ret = -EIO;
			fprintf(stderr, "Cannot open keydest file '%s'\n",
				params->keydest);
		}
	}

	ret = fdt_add_verif_data(params->keydir, params->keyfile, dest_blob,
				 blob, params->sig_name, params->comment,
				 params->require_keys, params->engine_id,
				 params->cmdname);
	if (ret) {
		if (ret != -ENOSPC)
			fprintf(stderr, "Failed to add signature\n");
		return ret;
	}

	if (outfile) {
		FILE *f;

		f = fopen(outfile, "wb");
		if (!f) {
			fprintf(stderr, "Cannot open output file '%s'\n",
				outfile);
			return -ENOENT;
		}
		if (fwrite(blob, size, 1, f) != 1) {
			fprintf(stderr, "Cannot write output file '%s'\n",
				outfile);
			return -EIO;
		}
		fclose(f);
	}

	return 0;
}

static int do_fdt_sign(struct image_tool_params *params, const char *cmdname,
		       const char *fdtfile, const char *outfile)
{
	struct stat fsbuf;
	int ffd, size_inc;
	bool in_place;
	void *blob;
	int ret;

	if (outfile) {
		if (copyfile(fdtfile, outfile) < 0) {
			printf("Can't copy %s to %s\n", fdtfile, outfile);
			return -EIO;
		}
		in_place = false;
	} else {
		outfile = fdtfile;
		in_place = true;
	}

	for (size_inc = 0; size_inc < 64 * 1024; size_inc += 1024) {
		ffd = mmap_fdt(cmdname, outfile, size_inc, &blob, &fsbuf,
			       !in_place, false);
		if (ffd < 0)
			return -1;

		ret = sign_fdt(params, 0, blob, fsbuf.st_size, outfile);
		if (!ret || ret != -ENOSPC)
			break;
	}

	(void)munmap((void *)blob, fsbuf.st_size);
	close(ffd);

	if (ret) {
		fprintf(stderr, "Failed to sign '%s' (error %d)\n",
			fdtfile, ret);
		return ret;
	}

	return 0;
}

int main(int argc, char **argv)
{
	struct image_tool_params params;
	char *fdtfile = NULL;
	char *outfile = NULL;
	char cmdname[256];
	int ret;
	int c;

	memset(&params, '\0', sizeof(params));
	strncpy(cmdname, *argv, sizeof(cmdname) - 1);
	cmdname[sizeof(cmdname) - 1] = '\0';
	while ((c = getopt(argc, argv, "f:G:k:K:o:S:")) != -1)
		switch (c) {
		case 'f':
			fdtfile = optarg;
			break;
		case 'G':
			params.keyfile = optarg;
			break;
		case 'k':
			params.keydir = optarg;
			break;
		case 'K':
			params.keydest = optarg;
			break;
		case 'o':
			outfile = optarg;
			break;
		case 'S':
			params.sig_name = optarg;
			break;
		default:
			usage(cmdname);
			break;
	}

	if (!fdtfile) {
		fprintf(stderr, "%s: Missing fdt file\n", *argv);
		usage(*argv);
	}
	if (!params.keyfile) {
		fprintf(stderr, "%s: Missing key file\n", *argv);
		usage(*argv);
	}

	ret = do_fdt_sign(&params, cmdname, fdtfile, outfile);

	exit(ret);
}
