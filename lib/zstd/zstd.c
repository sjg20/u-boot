// SPDX-License-Identifier: GPL-2.0+
/*
 * Copyright 2021 Google LLC
 */

#include <common.h>
#include <abuf.h>
#include <malloc.h>
#include <linux/zstd.h>

int zstd_decompress(struct abuf *in, struct abuf *out)
{
	ZSTD_DStream *dstream;
	ZSTD_inBuffer in_buf;
	ZSTD_outBuffer out_buf;
	void *workspace;
	size_t wsize;

	wsize = ZSTD_DStreamWorkspaceBound(abuf_size(in));
	workspace = malloc(wsize);
	if (!workspace) {
		debug("%s: cannot allocate workspace of size %zu\n", __func__,
			wsize);
		return -1;
	}

	dstream = ZSTD_initDStream(abuf_size(in), workspace, wsize);
	if (!dstream) {
		printf("%s: ZSTD_initDStream failed\n", __func__);
		return ZSTD_getErrorCode(0);
	}

	in_buf.src = abuf_data(in);
	in_buf.pos = 0;
	in_buf.size = abuf_size(in);

	out_buf.dst = abuf_data(out);
	out_buf.pos = 0;
	out_buf.size = abuf_size(out);

	while (1) {
		size_t ret;

		ret = ZSTD_decompressStream(dstream, &out_buf, &in_buf);
		if (ZSTD_isError(ret)) {
			printf("%s: ZSTD_decompressStream error %d\n", __func__,
				ZSTD_getErrorCode(ret));
			return ZSTD_getErrorCode(ret);
		}

		if (in_buf.pos >= abuf_size(in) || !ret)
			break;
	}

	return out_buf.pos;
}
