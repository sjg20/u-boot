#ifndef __LINUX_KCONFIG_H
#define __LINUX_KCONFIG_H

#ifdef USE_HOSTCC
#include <generated/autoconf_tools.h>
#elif defined(CONFIG_TPL_BUILD)
#include <generated/autoconf_tpl.h>
#elif defined(CONFIG_VPL_BUILD)
#include <generated/autoconf_vpl.h>
#elif defined(CONFIG_SPL_BUILD)
#include <generated/autoconf_spl.h>
#else
#include <generated/autoconf.h>
#endif

/*
 * Helper macros to use CONFIG_ options in C/CPP expressions. Note that
 * these only work with boolean and tristate options.
 */

/*
 * Getting something that works in C and CPP for an arg that may or may
 * not be defined is tricky.  Here, if we have "#define CONFIG_BOOGER 1"
 * we match on the placeholder define, insert the "0," for arg1 and generate
 * the triplet (0, 1, 0).  Then the last step cherry picks the 2nd arg (a one).
 * When CONFIG_BOOGER is not defined, we generate a (... 1, 0) pair, and when
 * the last step cherry picks the 2nd arg, we get a zero.
 */
#define __ARG_PLACEHOLDER_1 0,
#define config_enabled(cfg, def_val) _config_enabled(cfg, def_val)
#define _config_enabled(value, def_val) __config_enabled(__ARG_PLACEHOLDER_##value, def_val)
#define __config_enabled(arg1_or_junk, def_val) ___config_enabled(arg1_or_junk 1, def_val)
#define ___config_enabled(__ignored, val, ...) val

/*
 * Count number of arguments to a variadic macro. Currently only need
 * it for 1, 2 or 3 arguments.
 */
#define __arg6(a1, a2, a3, a4, a5, a6, ...) a6
#define __count_args(...) __arg6(dummy, ##__VA_ARGS__, 4, 3, 2, 1, 0)

#define __concat(a, b)   ___concat(a, b)
#define ___concat(a, b)  a ## b

#define __unwrap(...) __VA_ARGS__
#define __unwrap1(case1, case0) __unwrap case1
#define __unwrap0(case1, case0) __unwrap case0

#define __IS_ENABLED_1(option)        __IS_ENABLED_3(option, (1), (0))
#define __IS_ENABLED_2(option, case1) __IS_ENABLED_3(option, case1, ())
#define __IS_ENABLED_3(option, case1, case0) \
	__concat(__unwrap, config_enabled(option, 0)) (case1, case0)

/*
 * IS_ENABLED(CONFIG_FOO) returns 1 if CONFIG_FOO is enabled for the phase being
 * built, else 0.
 *
 * The optional second and third arguments must be parenthesized; that
 * allows one to include a trailing comma, e.g. for use in
 *
 * IS_ENABLED(CONFIG_ACME, ({.compatible = "acme,frobnozzle"},))
 *
 * which adds an entry to the array being defined if CONFIG_ACME is
 * set, and nothing otherwise.
 */

#define IS_ENABLED(option, ...) \
	__concat(__IS_ENABLED_, __count_args(option, ##__VA_ARGS__)) (option, ##__VA_ARGS__)

#ifndef __ASSEMBLY__
/*
 * Detect usage of the value when the conditional is not enabled. When used
 * in assembly context, this likely produces an assembly error, or hopefully at
 * least something recognisable.
 */
long invalid_use_of_IF_ENABLED_INT(void);
#endif

/* Evaluates to int_option if option is defined, otherwise build error */
#define IF_ENABLED_INT(option, int_option) \
	IS_ENABLED(option, (int_option), (invalid_use_of_IF_ENABLED_INT()))

#endif /* __LINUX_KCONFIG_H */
