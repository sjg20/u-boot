// SPDX-License-Identifier: GPL-2.0+
/*
 * (C) Copyright 2000-2013
 * Wolfgang Denk, DENX Software Engineering, wd@denx.de.
 *
 * (C) Copyright 2001 Sysgo Real-Time Solutions, GmbH <www.elinos.com>
 * Andreas Heppel <aheppel@sysgo.de>
 *
 * Copyright 2011 Freescale Semiconductor, Inc.
 */

#include <common.h>
#include <command.h>
#include <env.h>
#include <env_internal.h>
#include <malloc.h>
#include <asm/global_data.h>

DECLARE_GLOBAL_DATA_PTR;

/*
 * This variable is incremented on each do_env_set(), so it can
 * be used via env_get_id() as an indication, if the environment
 * has changed or not. So it is possible to reread an environment
 * variable only if the environment was changed ... done so for
 * example in NetInitLoop()
 */
static int env_id = 1;

int env_get_id(void)
{
	return env_id;
}

void env_inc_id(void)
{
	env_id++;
}

int _do_env_set(int flag, int argc, char *const argv[], int env_flag)
{
	int   i, len;
	char  *name, *value, *s;
	struct env_entry e, *ep;

	debug("Initial value for argc=%d\n", argc);

#if !IS_ENABLED(CONFIG_SPL_BUILD) && IS_ENABLED(CONFIG_CMD_NVEDIT_EFI)
	if (argc > 1 && argv[1][0] == '-' && argv[1][1] == 'e')
		return do_env_set_efi(NULL, flag, --argc, ++argv);
#endif

	while (argc > 1 && **(argv + 1) == '-') {
		char *arg = *++argv;

		--argc;
		while (*++arg) {
			switch (*arg) {
			case 'f':		/* force */
				env_flag |= H_FORCE;
				break;
			default:
				return CMD_RET_USAGE;
			}
		}
	}
	debug("Final value for argc=%d\n", argc);
	name = argv[1];

	if (strchr(name, '=')) {
		printf("## Error: illegal character '=' in variable name \"%s\"\n",
		       name);
		return 1;
	}

	env_id++;

	/* Delete only ? */
	if (argc < 3 || !argv[2]) {
		int rc = hdelete_r(name, &env_htab, env_flag);

		/* If the variable didn't exist, don't report an error */
		return rc && rc != -ENOENT ? 1 : 0;
	}

	/*
	 * Insert / replace new value
	 */
	for (i = 2, len = 0; i < argc; ++i)
		len += strlen(argv[i]) + 1;

	value = malloc(len);
	if (!value) {
		printf("## Can't malloc %d bytes\n", len);
		return 1;
	}
	for (i = 2, s = value; i < argc; ++i) {
		char *v = argv[i];

		while ((*s++ = *v++) != '\0')
			;
		*(s - 1) = ' ';
	}
	if (s != value)
		*--s = '\0';

	e.key	= name;
	e.data	= value;
	hsearch_r(e, ENV_ENTER, &ep, &env_htab, env_flag);
	free(value);
	if (!ep) {
		printf("## Error inserting \"%s\" variable, errno=%d\n",
		       name, errno);
		return 1;
	}

	return 0;
}

int env_set(const char *varname, const char *varvalue)
{
	const char * const argv[4] = { "setenv", varname, varvalue, NULL };

	/* before import into hashtable */
	if (!(gd->flags & GD_FLG_ENV_READY))
		return 1;

	if (!varvalue || !varvalue[0])
		return _do_env_set(0, 2, (char * const *)argv, H_PROGRAMMATIC);
	else
		return _do_env_set(0, 3, (char * const *)argv, H_PROGRAMMATIC);
}
