#include <image.h>
#include "fit_common.h"

static const char *cmdname;

static const char *algo_name = "sha1,rsa2048"; /* -a <algo> */
static const char *keydir = "."; /* -k <keydir> */
static const char *keyname = "key"; /* -n <keyname> */
static const char *require_keys; /* -r <conf|image> */
static const char *keydest; /* argv[n] */

static void usage(const char *msg)
{
	fprintf(stderr, "Error: %s\n", msg);
	fprintf(stderr, "Usage: %s [-a <algo>] [-k <keydir>] [-n <keyname>] [-r <conf|image>] <fdt blob>\n",
		cmdname);
	exit(EXIT_FAILURE);
}

static void process_args(int argc, char *argv[])
{
	int opt;

	while((opt = getopt(argc, argv, "a:k:n:r:")) != -1) {
		switch (opt) {
		case 'k':
			keydir = optarg;
			break;
		case 'a':
			algo_name = optarg;
			break;
		case 'n':
			keyname = optarg;
			break;
		case 'r':
			require_keys = optarg;
			break;
		default:
			usage("Invalid option");
		}
	}
	/* The last parameter is expected to be the .dtb to add the public key to */
	if (optind < argc)
		keydest = argv[optind];

	if (!keydest)
		usage("Missing dtb file to update");
}

int main(int argc, char *argv[])
{
	struct image_sign_info info;
	int signode, keynode, ret;
	void *dest_blob = NULL;
	struct stat dest_sbuf;
	size_t size_inc = 0;
	int destfd = -1;

	cmdname = argv[0];

	process_args(argc, argv);

	memset(&info, 0, sizeof(info));

	info.keydir = keydir;
	info.keyname = keyname;
	info.name = algo_name;
	info.require_keys = require_keys;
	info.crypto = image_get_crypto_algo(algo_name);
	if (!info.crypto) {
                fprintf(stderr, "Unsupported signature algorithm '%s'\n", algo_name);
		exit(EXIT_FAILURE);
	}

	do {
		if (destfd >= 0) {
			munmap(dest_blob, dest_sbuf.st_size);
			close(destfd);

			fprintf(stderr, ".dtb too small, increasing size by 1024 bytes\n");
			size_inc = 1024;
		}

		destfd = mmap_fdt(cmdname, keydest, size_inc, &dest_blob, &dest_sbuf, false, false);
		if (destfd < 0)
			exit(EXIT_FAILURE);

		ret = info.crypto->add_verify_data(&info, dest_blob);
		if (ret == -ENOSPC)
			continue;
		else if (ret)
			break;

		signode = fdt_path_offset(dest_blob, "/signature");
		if (signode < 0) {
			fprintf(stderr, "%s: /signature node not found?!\n",
				cmdname);
			exit(EXIT_FAILURE);
		}

		keynode = fdt_first_subnode(dest_blob, signode);
		if (keynode < 0) {
			fprintf(stderr, "%s: /signature/<key> node not found?!\n",
				cmdname);
			exit(EXIT_FAILURE);
		}

		ret = fdt_appendprop(dest_blob, keynode, "u-boot,dm-spl", NULL, 0);
	} while (ret == -ENOSPC);

	if (ret) {
		fprintf(stderr, "%s: Cannot add public key to FIT blob: %s\n",
			cmdname, strerror(-ret));
		exit(EXIT_FAILURE);
	}

	exit(EXIT_SUCCESS);
}
