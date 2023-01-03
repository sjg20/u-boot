// SPDX-License-Identifier: GPL-2.0+
/*
 * Copyright (c) 2013 Google, Inc
 */

/*
 * Decode and dump U-Boot profiling information into a format that can be used
 * by kernelshark
 */

#include <assert.h>
#include <ctype.h>
#include <limits.h>
#include <regex.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/param.h>
#include <sys/types.h>

#include <compiler.h>
#include <trace.h>
#include <abuf.h>

/* Set to 1 to emit version 7 file (currently only partly supported) */
#define VERSION7	0

#define MAX_LINE_LEN 500

/* from linux/kernel.h */
#define __ALIGN_MASK(x,mask)	(((x)+(mask))&~(mask))
#define ALIGN(x,a)		__ALIGN_MASK((x),(typeof(x))(a)-1)

enum {
	FUNCF_TRACE	= 1 << 0,	/* Include this function in trace */
	TRACE_PAGE_SIZE	= 4096,		/* Assumed page size for trace */
	TRACE_PID	= 1,		/* PID to use for U-Boot */
	LEN_STACK_SIZE	= 4,		/* number of nested length fix-ups */
	TRACE_PAGE_MASK	= TRACE_PAGE_SIZE - 1,
};

/* Section types */
enum {
	SECTION_OPTIONS,
};

/* Option types */
enum {
	OPTION_DONE,
	OPTION_DATE,
	OPTION_CPUSTAT,
	OPTION_BUFFER,
	OPTION_TRACECLOCK,
	OPTION_UNAME,
	OPTION_HOOK,
	OPTION_OFFSET,
	OPTION_CPUCOUNT,
	OPTION_VERSION,
	OPTION_PROCMAPS,
	OPTION_TRACEID,
	OPTION_TIME_SHIFT,
	OPTION_GUEST,
	OPTION_TSC2NSEC,
};

/* types of trace records */
enum trace_type {
	__TRACE_FIRST_TYPE = 0,

	TRACE_FN,
	TRACE_CTX,
	TRACE_WAKE,
	TRACE_STACK,
	TRACE_PRINT,
	TRACE_BPRINT,
	TRACE_MMIO_RW,
	TRACE_MMIO_MAP,
	TRACE_BRANCH,
	TRACE_GRAPH_RET,
	TRACE_GRAPH_ENT,
};

struct func_info {
	unsigned long offset;
	const char *name;
	unsigned long code_size;
	unsigned long call_count;
	unsigned flags;
	/* the section this function is in */
	struct objsection_info *objsection;
};

enum trace_line_type {
	TRACE_LINE_INCLUDE,
	TRACE_LINE_EXCLUDE,
};

struct trace_configline_info {
	struct trace_configline_info *next;
	enum trace_line_type type;
	const char *name;	/* identifier name / wildcard */
	regex_t regex;		/* Regex to use if name starts with / */
};

/**
 * struct tw_len - holds information about a length that needs updating
 *
 * This is used to record a placeholder for a u64 length which needs to be
 * updated once the length is known
 *
 * This allows us to write tw->ptr - @len_base to position @ptr in the file
 *
 * @ptr: Position of the length value in the file
 * @base: Base position for the calculation
 * @size: Size of the length value, in bytes (4 or 8)
 */
struct tw_len {
	int ptr;
	int base;
	int size;
};

struct twriter {
	int ptr;
	struct tw_len len_stack[LEN_STACK_SIZE];
	int len_count;
	struct abuf str_buf;
	int str_ptr;
	FILE *fout;
};

/* The contents of the trace config file */
struct trace_configline_info *trace_config_head;

struct func_info *func_list;
int func_count;
struct trace_call *call_list;
int call_count;
int verbose;	/* Verbosity level 0=none, 1=warn, 2=notice, 3=info, 4=debug */
unsigned long text_offset;		/* text address of first function */

static void outf(int level, const char *fmt, ...)
		__attribute__ ((format (__printf__, 2, 3)));
#define error(fmt, b...) outf(0, fmt, ##b)
#define warn(fmt, b...) outf(1, fmt, ##b)
#define notice(fmt, b...) outf(2, fmt, ##b)
#define info(fmt, b...) outf(3, fmt, ##b)
#define debug(fmt, b...) outf(4, fmt, ##b)


static void outf(int level, const char *fmt, ...)
{
	if (verbose >= level) {
		va_list args;

		va_start(args, fmt);
		vfprintf(stderr, fmt, args);
		va_end(args);
	}
}

static void usage(void)
{
	fprintf(stderr,
		"Usage: proftool [-cmtv] <cmd> <profdata>\n"
		"\n"
		"Commands\n"
		"   dump-ftrace\t\tDump out textual data in ftrace format\n"
		"\n"
		"Options:\n"
		"   -c <cfg>\tSpecify config file\n"
		"   -m <map>\tSpecify Systen.map file\n"
		"   -o <fname>\tSpecify output file\n"
		"   -t <fname>\tSpecify trace data file (from U-Boot 'trace calls')\n"
		"   -v <0-4>\tSpecify verbosity\n");
	exit(EXIT_FAILURE);
}

static int h_cmp_offset(const void *v1, const void *v2)
{
	const struct func_info *f1 = v1, *f2 = v2;

	return (f1->offset / FUNC_SITE_SIZE) - (f2->offset / FUNC_SITE_SIZE);
}

static int read_system_map(FILE *fin)
{
	unsigned long offset, start = 0;
	struct func_info *func;
	char buff[MAX_LINE_LEN];
	char symtype;
	char symname[MAX_LINE_LEN + 1];
	int linenum;
	int alloced;

	for (linenum = 1, alloced = func_count = 0;; linenum++) {
		int fields = 0;

		if (fgets(buff, sizeof(buff), fin))
			fields = sscanf(buff, "%lx %c %100s\n", &offset,
				&symtype, symname);
		if (fields == 2) {
			continue;
		} else if (feof(fin)) {
			break;
		} else if (fields < 2) {
			error("Map file line %d: invalid format\n", linenum);
			return 1;
		}

		/* Must be a text symbol */
		symtype = tolower(symtype);
		if (symtype != 't' && symtype != 'w')
			continue;

		if (func_count == alloced) {
			alloced += 256;
			func_list = realloc(func_list,
					sizeof(struct func_info) * alloced);
			assert(func_list);
		}
		if (!func_count)
			start = offset;

		func = &func_list[func_count++];
		memset(func, '\0', sizeof(*func));
		func->offset = offset - start;
		func->name = strdup(symname);
		func->flags = FUNCF_TRACE;	/* trace by default */

		/* Update previous function's code size */
		if (func_count > 1)
			func[-1].code_size = func->offset - func[-1].offset;
	}
	notice("%d functions found in map file, start addr %lx\n", func_count,
	       start);
	text_offset = start;

	return 0;
}

static int read_data(FILE *fin, void *buff, int size)
{
	int err;

	err = fread(buff, 1, size, fin);
	if (!err)
		return 1;
	if (err != size) {
		error("Cannot read profile file at pos %lx\n", ftell(fin));
		return -1;
	}
	return 0;
}

static struct func_info *find_func_by_offset(uint32_t offset)
{
	struct func_info key, *found;

	key.offset = offset;
	found = bsearch(&key, func_list, func_count, sizeof(struct func_info),
			h_cmp_offset);

	return found;
}

/* This finds the function which contains the given offset */
static struct func_info *find_caller_by_offset(uint32_t offset)
{
	int low;	/* least function that could be a match */
	int high;	/* greated function that could be a match */
	struct func_info key;

	low = 0;
	high = func_count - 1;
	key.offset = offset;
	while (high > low + 1) {
		int mid = (low + high) / 2;
		int result;

		result = h_cmp_offset(&key, &func_list[mid]);
		if (result > 0)
			low = mid;
		else if (result < 0)
			high = mid;
		else
			return &func_list[mid];
	}

	return low >= 0 ? &func_list[low] : NULL;
}

static int read_calls(FILE *fin, size_t count)
{
	struct trace_call *call_data;
	int i;

	notice("call count: %zu\n", count);
	call_list = (struct trace_call *)calloc(count, sizeof(*call_data));
	if (!call_list) {
		error("Cannot allocate call_list\n");
		return -1;
	}
	call_count = count;

	call_data = call_list;
	for (i = 0; i < count; i++, call_data++) {
		if (read_data(fin, call_data, sizeof(*call_data)))
			return 1;
	}
	return 0;
}

static int read_profile(FILE *fin, int *not_found)
{
	struct trace_output_hdr hdr;

	*not_found = 0;
	while (!feof(fin)) {
		int err;

		err = read_data(fin, &hdr, sizeof(hdr));
		if (err == 1)
			break; /* EOF */
		else if (err)
			return 1;

		switch (hdr.type) {
		case TRACE_CHUNK_FUNCS:
			/* Ignored at present */
			break;

		case TRACE_CHUNK_CALLS:
			if (read_calls(fin, hdr.rec_count))
				return 1;
			break;
		}
	}
	return 0;
}

static int read_map_file(const char *fname)
{
	FILE *fmap;
	int err = 0;

	fmap = fopen(fname, "r");
	if (!fmap) {
		error("Cannot open map file '%s'\n", fname);
		return 1;
	}
	if (fmap) {
		err = read_system_map(fmap);
		fclose(fmap);
	}
	return err;
}

static int read_profile_file(const char *fname)
{
	int not_found = INT_MAX;
	FILE *fprof;
	int err;

	fprof = fopen(fname, "rb");
	if (!fprof) {
		error("Cannot open profile data file '%s'\n",
		      fname);
		return 1;
	} else {
		err = read_profile(fprof, &not_found);
		fclose(fprof);
		if (err)
			return err;

		if (not_found) {
			warn("%d profile functions could not be found in the map file - are you sure that your profile data and map file correspond?\n",
			     not_found);
			return 1;
		}
	}
	return 0;
}

static int regex_report_error(regex_t *regex, int err, const char *op,
			      const char *name)
{
	char buf[200];

	regerror(err, regex, buf, sizeof(buf));
	error("Regex error '%s' in %s '%s'\n", buf, op, name);
	return -1;
}

static void check_trace_config_line(struct trace_configline_info *item)
{
	struct func_info *func, *end;
	int err;

	debug("Checking trace config line '%s'\n", item->name);
	for (func = func_list, end = func + func_count; func < end; func++) {
		err = regexec(&item->regex, func->name, 0, NULL, 0);
		debug("   - regex '%s', string '%s': %d\n", item->name,
		      func->name, err);
		if (err == REG_NOMATCH)
			continue;

		if (err) {
			regex_report_error(&item->regex, err, "match",
					   item->name);
			break;
		}

		/* It matches, so perform the action */
		switch (item->type) {
		case TRACE_LINE_INCLUDE:
			info("      include %s at %lx\n", func->name,
			     text_offset + func->offset);
			func->flags |= FUNCF_TRACE;
			break;

		case TRACE_LINE_EXCLUDE:
			info("      exclude %s at %lx\n", func->name,
			     text_offset + func->offset);
			func->flags &= ~FUNCF_TRACE;
			break;
		}
	}
}

static void check_trace_config(void)
{
	struct trace_configline_info *line;

	for (line = trace_config_head; line; line = line->next)
		check_trace_config_line(line);
}

/**
 * Check the functions to see if they each have an objsection. If not, then
 * the linker must have eliminated them.
 */
static void check_functions(void)
{
	struct func_info *func, *end;
	unsigned long removed_code_size = 0;
	int not_found = 0;

	/* Look for missing functions */
	for (func = func_list, end = func + func_count; func < end; func++) {
		if (!func->objsection) {
			removed_code_size += func->code_size;
			not_found++;
		}
	}

	/* Figure out what functions we want to trace */
	check_trace_config();

	warn("%d functions removed by linker, %ld code size\n",
	     not_found, removed_code_size);
}

static int read_trace_config(FILE *fin)
{
	char buff[200];
	int linenum = 0;
	struct trace_configline_info **tailp = &trace_config_head;

	while (fgets(buff, sizeof(buff), fin)) {
		int len = strlen(buff);
		struct trace_configline_info *line;
		char *saveptr;
		char *s, *tok;
		int err;

		linenum++;
		if (len && buff[len - 1] == '\n')
			buff[len - 1] = '\0';

		/* skip blank lines and comments */
		for (s = buff; *s == ' ' || *s == '\t'; s++)
			;
		if (!*s || *s == '#')
			continue;

		line = (struct trace_configline_info *)calloc(1,
							      sizeof(*line));
		if (!line) {
			error("Cannot allocate config line\n");
			return -1;
		}

		tok = strtok_r(s, " \t", &saveptr);
		if (!tok) {
			error("Invalid trace config data on line %d\n",
			      linenum);
			return -1;
		}
		if (0 == strcmp(tok, "include-func")) {
			line->type = TRACE_LINE_INCLUDE;
		} else if (0 == strcmp(tok, "exclude-func")) {
			line->type = TRACE_LINE_EXCLUDE;
		} else {
			error("Unknown command in trace config data line %d\n",
			      linenum);
			return -1;
		}

		tok = strtok_r(NULL, " \t", &saveptr);
		if (!tok) {
			error("Missing pattern in trace config data line %d\n",
			      linenum);
			return -1;
		}

		err = regcomp(&line->regex, tok, REG_NOSUB);
		if (err) {
			int r = regex_report_error(&line->regex, err,
						   "compile", tok);
			free(line);
			return r;
		}

		/* link this new one to the end of the list */
		line->name = strdup(tok);
		line->next = NULL;
		*tailp = line;
		tailp = &line->next;
	}

	if (!feof(fin)) {
		error("Cannot read from trace config file at position %ld\n",
		      ftell(fin));
		return -1;
	}
	return 0;
}

static int read_trace_config_file(const char *fname)
{
	FILE *fin;
	int err;

	fin = fopen(fname, "r");
	if (!fin) {
		error("Cannot open trace_config file '%s'\n", fname);
		return -1;
	}
	err = read_trace_config(fin);
	fclose(fin);
	return err;
}
/*
static void out_func(ulong func_offset, int is_caller, const char *suffix)
{
	struct func_info *func;

	func = (is_caller ? find_caller_by_offset : find_func_by_offset)
		(func_offset);

	if (func)
		printf("%s%s", func->name, suffix);
	else
		printf("%lx%s", func_offset, suffix);
}
*/
static int tputh(FILE *fout, unsigned int val)
{
	fputc(val, fout);
	fputc(val >> 8, fout);

	return 2;
}

static int tputl(FILE *fout, ulong val)
{
	fputc(val, fout);
	fputc(val >> 8, fout);
	fputc(val >> 16, fout);
	fputc(val >> 24, fout);

	return 4;
}

static int tputq(FILE *fout, unsigned long long val)
{
	tputl(fout, val);
	tputl(fout, val >> 32U);

	return 8;
}

static int tputs(FILE *fout, const char *str)
{
	fputs(str, fout);

	return strlen(str);
}

static int add_str(struct twriter *tw, const char *name)
{
	int str_ptr;
	int len;

	len = strlen(name) + 1;
	str_ptr = tw->str_ptr;
	tw->str_ptr += len;

	if (tw->str_ptr > abuf_size(&tw->str_buf)) {
		int new_size;

		new_size = ALIGN(tw->str_ptr, 4096);
		if (!abuf_realloc(&tw->str_buf, new_size))
			return -1;
	}

	return str_ptr;
}

/**
 * push_len() - Push a new length request onto the stack
 *
 * @tw: Writer context
 * @base: Base position of the length calculation
 * @msg: Indicates the type of caller, for debugging
 * @size: Size of the length value, either 4 bytes or 8
 * Returns number of bytes written to the file (=@size on success), -ve on error
 *
 * This marks a place where a length must be written, covering data that is
 * about to be written. It writes a placeholder value.
 *
 * Once the data is written, calling pop_len() will update the placeholder with
 * the correct length based on how many bytes have been written
 */
static int push_len(struct twriter *tw, int base, const char *msg, int size)
{
	struct tw_len *lp;

	if (tw->len_count >= LEN_STACK_SIZE) {
		fprintf(stderr, "Length-stack overflow: %s\n", msg);
		return -1;
	}
	if (size != 4 && size != 8) {
		fprintf(stderr, "Length-stack invalid size %d: %s\n", size,
			msg);
		return -1;
	}

	lp = &tw->len_stack[tw->len_count++];
	lp->base = base;
	lp->ptr = tw->ptr;
	lp->size = size;

	return size == 8 ? tputq(tw->fout, 0) : tputl(tw->fout, 0);
}

static int pop_len(struct twriter *tw, const char *msg)
{
	struct tw_len *lp;
	int len, ret;

	if (!tw->len_count) {
		fprintf(stderr, "Length-stack underflow: %s\n", msg);
		return -1;
	}

	lp = &tw->len_stack[--tw->len_count];
	if (fseek(tw->fout, lp->ptr, SEEK_SET))
		return -1;
	len = tw->ptr - lp->base;
	ret = lp->size == 8 ? tputq(tw->fout, len) : tputl(tw->fout, len);
	if (ret < 0)
		return -1;
	if (fseek(tw->fout, tw->ptr, SEEK_SET))
		return -1;

	return 0;
}

static int start_header(struct twriter *tw, int id, int flags, const char *name)
{
	int str_id;
	int lptr;
	int base;
	int ret;

	base = tw->ptr + 16;
	lptr = 0;
	lptr += tputh(tw->fout, id);
	lptr += tputh(tw->fout, flags);
	str_id = add_str(tw, name);
	if (str_id < 0)
		return -1;
	lptr += tputl(tw->fout, str_id);

	/* placeholder for size */
	ret = push_len(tw, base, "v7 header", 8);
	if (ret < 0)
		return -1;
	lptr += ret;

	return lptr;
}

static int start_page(struct twriter *tw, ulong timestamp)
{
	int start;
	int ret;

	/* move to start of next page */
	start = ALIGN(tw->ptr, TRACE_PAGE_SIZE);
	ret = fseek(tw->fout, start, SEEK_SET);
	if (ret < 0) {
		fprintf(stderr, "Cannot seek to page start\n");
		return -1;
	}
	tw->ptr = start;

	/* page header */
	tw->ptr += tputq(tw->fout, timestamp);
	ret = push_len(tw, start + 16, "page", 8);
	if (ret < 0)
		return ret;
	tw->ptr += ret;

	return 0;
}

static int finish_page(struct twriter *tw)
{
	int ret, end;

	ret = pop_len(tw, "page");
	if (ret < 0)
		return ret;
	end = ALIGN(tw->ptr, TRACE_PAGE_SIZE);
	if (fseek(tw->fout, end - 1, SEEK_SET)) {
		fprintf(stderr, "cannot seek to start of next page\n");
		return -1;
	}
	fputc(0, tw->fout);
	tw->ptr = end;

	return 0;
}

/*
 * See here for format:
 *
 * https://github.com/rostedt/trace-cmd/blob/master/Documentation/trace-cmd/trace-cmd.dat.v7.5.txt
 */
static int make_ftrace(FILE *fout)
{
	struct trace_call *call;
	int missing_count = 0, skip_count = 0;
	struct twriter tws, *tw = &tws;
	int i, ret, len, start, upto;
	bool in_page;
	char str[800];
	ulong base_timestamp;
	int page_upto, depth, min_depth;

	memset(tw, '\0', sizeof(*tw));
	abuf_init(&tw->str_buf);
	tw->fout = fout;

	tw->ptr = 0;
	tw->ptr += fprintf(fout, "%c%c%ctracing6%c%c%c", 0x17, 0x08, 0x44,
			   0 /* terminator */, 0 /* little endian */,
			   4 /* 32-bit long values */);

	/* host-machine page size 4KB */
	tw->ptr += tputl(fout, 4 << 10);

	tw->ptr += fprintf(fout, "header_page%c", 0);

	snprintf(str, sizeof(str),
		"\tfield: u64 timestamp;\toffset:0;\tsize:8;\tsigned:0;\n"
		"\tfield: local_t commit;\toffset:8;\tsize:8;\tsigned:1;\n"
		"\tfield: int overwrite;\toffset:8;\tsize:1;\tsigned:1;\n"
		"\tfield: char data;\toffset:16;\tsize:4080;\tsigned:1;\n");
	len = strlen(str);
	tw->ptr += tputq(fout, len);
	tw->ptr += tputs(fout, str);

	if (VERSION7) {
		/* no compression */
		tw->ptr += fprintf(fout, "none%cversion%c\n", 0, 0);

		ret = start_header(tw, SECTION_OPTIONS, 0, "options");
		if (ret < 0) {
			fprintf(stderr, "Cannot start option header\n");
			return -1;
		}
		tw->ptr += ret;
		tw->ptr += tputh(fout, OPTION_DONE);
		tw->ptr += tputl(fout, 8);
		tw->ptr += tputl(fout, 0);
		ret = pop_len(tw, "t7 header");
		if (ret < 0) {
			fprintf(stderr, "Cannot finish option header\n");
			return -1;
		}
	}

	tw->ptr += fprintf(fout, "header_event%c", 0);
	snprintf(str, sizeof(str),
		"# compressed entry header\n"
		"\ttype_len    :    5 bits\n"
		"\ttime_delta  :   27 bits\n"
		"\tarray       :   32 bits\n"
		"\n"
		"\tpadding     : type == 29\n"
		"\ttime_extend : type == 30\n"
		"\ttime_stamp : type == 31\n"
		"\tdata max type_len  == 28\n");
	len = strlen(str);
	tw->ptr += tputq(fout, len);
	tw->ptr += tputs(fout, str);

	/* number of ftrace-event-format files */
	tw->ptr += tputl(fout, 3);

	snprintf(str, sizeof(str),
		 "name: function\n"
		 "ID: 1\n"
		 "format:\n"
		 "\tfield:unsigned short common_type;\toffset:0;\tsize:2;\tsigned:0;\n"
		 "\tfield:unsigned char common_flags;\toffset:2;\tsize:1;\tsigned:0;\n"
		 "\tfield:unsigned char common_preempt_count;\toffset:3;\tsize:1;signed:0;\n"
		 "\tfield:int common_pid;\toffset:4;\tsize:4;\tsigned:1;\n"
		 "\n"
		 "\tfield:unsigned long ip;\toffset:8;\tsize:8;\tsigned:0;\n"
		 "\tfield:unsigned long parent_ip;\toffset:16;\tsize:8;\tsigned:0;\n"
		 "\n"
		 "print fmt: \" %%ps <-- %%ps\", (void *)REC->ip, (void *)REC->parent_ip\n");
	len = strlen(str);
	tw->ptr += tputq(fout, len);
	tw->ptr += tputs(fout, str);

	snprintf(str, sizeof(str),
		 "name: funcgraph_entry\n"
		 "ID: 11\n"
		 "format:\n"
		 "\tfield:unsigned short common_type;\toffset:0;\tsize:2;\tsigned:0;\n"
		 "\tfield:unsigned char common_flags;\toffset:2;\tsize:1;\tsigned:0;\n"
		 "\tfield:unsigned char common_preempt_count;\toffset:3;\tsize:1;signed:0;\n"
		 "\tfield:int common_pid;\toffset:4;\tsize:4;\tsigned:1;\n"
		 "\n"
		 "\tfield:unsigned long func;\toffset:8;\tsize:8;\tsigned:0;\n"
		 "\tfield:int depth;\toffset:16;\tsize:4;\tsigned:1;\n"
		"\n"
		 "print fmt: \"--> %%ps (%%d)\", (void *)REC->func, REC->depth\n");
	len = strlen(str);
	tw->ptr += tputq(fout, len);
	tw->ptr += tputs(fout, str);

	snprintf(str, sizeof(str),
		"name: funcgraph_exit\n"
		"ID: 10\n"
		"format:\n"
		"\tfield:unsigned short common_type;\toffset:0;\tsize:2;\tsigned:0;\n"
		"\tfield:unsigned char common_flags;\toffset:2;\tsize:1;\tsigned:0;\n"
		"\tfield:unsigned char common_preempt_count;\toffset:3;\tsize:1;signed:0;\n"
		"\tfield:int common_pid;\toffset:4;\tsize:4;\tsigned:1;\n"
		"\n"
		"\tfield:unsigned long func;\toffset:8;\tsize:8;\tsigned:0;\n"
		"\tfield:int depth;\toffset:16;\tsize:4;\tsigned:1;\n"
		"\tfield:unsigned int overrun;\toffset:20;\tsize:4;\tsigned:0;\n"
		"\tfield:unsigned long long calltime;\toffset:24;\tsize:8;\tsigned:0;\n"
		"\tfield:unsigned long long rettime;\toffset:32;\tsize:8;\tsigned:0;\n"
		"\n"
		"print fmt: \"<-- %%ps (%%d) (start: %%llx  end: %%llx) over: %%d\", (void *)REC->func, REC->depth, REC->calltime, REC->rettime, REC->depth\n");
	len = strlen(str);
	tw->ptr += tputq(fout, len);
	tw->ptr += tputs(fout, str);

	/* number of event systems files */
	tw->ptr += tputl(fout, 0);

	/* write symbols */
	ret = push_len(tw, tw->ptr + 4, "syms", 4);
	if (ret < 0)
		return -1;
	tw->ptr += ret;
	printf("func_count %d\n", func_count);
	for (i = 0; i < func_count; i++) {
		struct func_info *func = &func_list[i];

		snprintf(str, sizeof(str), "%016lx T %s\n",
			 text_offset + func->offset, func->name);
		if (i < 10)
			printf("%d: %s", i, str);
		len = strlen(str);
		tw->ptr += tputs(fout, str);
	}
	ret = pop_len(tw, "syms");
	if (ret < 0) {
		fprintf(stderr, "Cannot finish sym header\n");
		return -1;
	}
	tw->ptr += ret;
	printf("check %x %x\n", tw->ptr, ftell(tw->fout));

	/* trace_printk, 0 for now */
	tw->ptr += tputl(fout, 0);

	/* processes */
	snprintf(str, sizeof(str), "%d u-boot\n", TRACE_PID);
	len = strlen(str);
	tw->ptr += tputq(fout, len);
	tw->ptr += tputs(fout, str);

	/* number of CPUs */
	tw->ptr += tputl(fout, 1);

	tw->ptr += fprintf(fout, "options  %c", 0);

	/* traceclock */
	tw->ptr += tputh(fout, OPTION_TRACECLOCK);
	tw->ptr += tputl(fout, 0);

	/* uname */
	tw->ptr += tputh(fout, OPTION_UNAME);
	snprintf(str, sizeof(str), "U-Boot");
	len = strlen(str);
	tw->ptr += tputl(fout, len);
	tw->ptr += tputs(fout, str);

	/* version */
	tw->ptr += tputh(fout, OPTION_VERSION);
	snprintf(str, sizeof(str), "unknown");
	len = strlen(str);
	tw->ptr += tputl(fout, len);
	tw->ptr += tputs(fout, str);

	/* trace ID */
	tw->ptr += tputh(fout, OPTION_TRACEID);
	tw->ptr += tputl(fout, 8);
	tw->ptr += tputq(fout, 0x123456780abcdef0);

	/* time conversion */
	tw->ptr += tputh(fout, OPTION_TSC2NSEC);
	tw->ptr += tputl(fout, 16);
	tw->ptr += tputl(fout, 1000);	/* multiplier */
	tw->ptr += tputl(fout, 1);	/* shift */
	tw->ptr += tputq(fout, 0);	/* offset */

	/* cpustat */
	tw->ptr += tputh(fout, OPTION_CPUSTAT);
	snprintf(str, sizeof(str),
		 "CPU: 0\n"
		 "entries: 100\n"
		 "overrun: 43565\n"
		 "commit overrun: 0\n"
		 "bytes: 3360\n"
		 "oldest event ts: 963732.447752\n"
		 "now ts: 963832.146824\n"
		 "dropped events: 0\n"
		 "read events: 42379\n");
	len = strlen(str);
	tw->ptr += tputl(fout, len);
	tw->ptr += tputs(fout, str);

	tw->ptr += tputh(fout, OPTION_DONE);

	tw->ptr += fprintf(fout, "flyrecord%c", 0);

	/* trace data */
	start = ALIGN(tw->ptr + 16, TRACE_PAGE_SIZE);
	tw->ptr += tputq(fout, start);

	/* use a placeholder for the size */
	ret = push_len(tw, start, "flyrecord", 8);
	if (ret < 0)
		return -1;
	tw->ptr += ret;

	snprintf(str, sizeof(str),
		 "[local] global counter uptime perf mono mono_raw boot x86-tsc\n");
	len = strlen(str);
	tw->ptr += tputq(fout, len);
	tw->ptr += tputs(fout, str);

	in_page = false;
	base_timestamp = 0;
	upto = 0;
	page_upto = 0;

	/* Calculate minimum depth */
	depth = 0;
	min_depth = 0;
	for (i = 0, call = call_list; i < call_count; i++, call++) {
		switch (TRACE_CALL_TYPE(call)) {
		case FUNCF_ENTRY:
			depth++;
			break;
		case FUNCF_EXIT:
			depth--;
			if (depth < min_depth)
				min_depth = depth;
			break;
		}
	}

	depth = -min_depth;
	for (i = 0, call = call_list; i < call_count; i++, call++) {
		struct func_info *func;
// 		struct func_info *caller_func;
		ulong timestamp;
		int delta;
		bool entry;
		int rec_words;

		if (TRACE_CALL_TYPE(call) != FUNCF_ENTRY &&
		    TRACE_CALL_TYPE(call) != FUNCF_EXIT)
			continue;
		func = find_func_by_offset(call->func);
		if (!func) {
			warn("Cannot find function at %lx\n",
			     text_offset + call->func);
			missing_count++;
			continue;
		}

		if (!(func->flags & FUNCF_TRACE)) {
			debug("Funcion '%s' is excluded from trace\n",
			      func->name);
			skip_count++;
			continue;
		}

#if 0
		rec_words = 6;
#else
		entry = TRACE_CALL_TYPE(call) == FUNCF_ENTRY;

		/* 2 header words and then 3 or 8 others */
		rec_words = 2 + (entry ? 3 : 8);
#endif

		/* convert timestamp from us to ns */
		timestamp = (call->flags & FUNCF_TIMESTAMP_MASK);
		if (in_page) {
			if (page_upto + rec_words * 4 > TRACE_PAGE_SIZE) {
				if (finish_page(tw))
					return -1;
				in_page = false;
			}
		}
		if (!in_page) {
			if (start_page(tw, timestamp))
				return -1;
			in_page = true;
			base_timestamp = timestamp;
			page_upto = tw->ptr & TRACE_PAGE_MASK;
		}

		delta = timestamp - base_timestamp;
		if (delta > 0x07fffff) {
			/*
			 * hard to imagine how this could happen since it means
			 * that no function calls were made for a long time
			 */
			fprintf(stderr, "cannot represent time delta\n");
			return -1;
		}
#if 0
		/* type_len is 6, meaning 4 * 6 = 24 bytes */
		tw->ptr += tputl(fout, rec_words | delta << 5);
		tw->ptr += tputh(fout, TRACE_FN);
		tw->ptr += tputh(fout, 0);	/* flags */
		tw->ptr += tputl(fout, TRACE_PID);	/* PID */
		tw->ptr += tputq(fout, text_offset + func->offset);	/* function */
		caller_func = find_caller_by_offset(call->caller);
		tw->ptr += tputq(fout, caller_func->offset);	/* caller */
#else
		tw->ptr += tputl(fout, rec_words | delta << 5);
		if (entry) {
			tw->ptr += tputh(fout, TRACE_GRAPH_ENT);
			tw->ptr += tputh(fout, 0);	/* flags */
			tw->ptr += tputl(fout, TRACE_PID);	/* PID */
			tw->ptr += tputq(fout, text_offset + func->offset);	/* function */
			tw->ptr += tputl(fout, depth++);	/* depth */
		} else {
			if (depth)
				depth--;
			tw->ptr += tputh(fout, TRACE_GRAPH_RET);
			tw->ptr += tputh(fout, 0);	/* flags */
			tw->ptr += tputl(fout, TRACE_PID);	/* PID */
			tw->ptr += tputq(fout, text_offset + func->offset);	/* function */
			tw->ptr += tputl(fout, depth);	/* depth */
			tw->ptr += tputl(fout, 0);	/* overrun */
			tw->ptr += tputq(fout, 0);	/* calltime */
			tw->ptr += tputq(fout, 0);	/* rettime */
		}
#endif
		page_upto += 4 + rec_words * 4;
		upto++;
		if (upto == 200)
			break;
	}
	if (in_page && finish_page(tw))
		return -1;

	ret = pop_len(tw, "flyrecord");
	if (ret < 0) {
		fprintf(stderr, "Cannot finish flyrecord header\n");
		return -1;
	}

	info("ftrace: %d functions not found, %d excluded\n", missing_count,
	     skip_count);

	return 0;
}

static int prof_tool(int argc, char *const argv[],
		     const char *prof_fname, const char *map_fname,
		     const char *trace_config_fname, const char *out_fname)
{
	int err = 0;

	if (read_map_file(map_fname))
		return -1;
	if (prof_fname && read_profile_file(prof_fname))
		return -1;
	if (trace_config_fname && read_trace_config_file(trace_config_fname))
		return -1;

	check_functions();

	for (; argc; argc--, argv++) {
		const char *cmd = *argv;

		if (0 == strcmp(cmd, "dump-ftrace")) {
			FILE *fout;

			fout = fopen(out_fname, "w");
			if (!fout) {
				fprintf(stderr, "Cannot write file '%s'\n",
					out_fname);
				return -1;
			}
			err = make_ftrace(fout);
			fclose(fout);
		} else {
			warn("Unknown command '%s'\n", cmd);
		}
	}

	return err;
}

int main(int argc, char *argv[])
{
	const char *map_fname = "System.map";
	const char *trace_fname = NULL;
	const char *config_fname = NULL;
	const char *out_fname = NULL;
	int opt;

	verbose = 2;
	while ((opt = getopt(argc, argv, "c:m:o:t:v:")) != -1) {
		switch (opt) {
		case 'c':
			config_fname = optarg;
			break;
		case 'm':
			map_fname = optarg;
			break;
		case 'o':
			out_fname = optarg;
			break;
		case 't':
			trace_fname = optarg;
			break;
		case 'v':
			verbose = atoi(optarg);
			break;

		default:
			usage();
		}
	}
	argc -= optind; argv += optind;
	if (argc < 1)
		usage();

	debug("Debug enabled\n");
	return prof_tool(argc, argv, trace_fname, map_fname, config_fname,
			 out_fname);
}
