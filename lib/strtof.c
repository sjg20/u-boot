// SPDX-License-Identifier: GPL-2.0+
/*
 * Copyright (c) 1988-1993 The Regents of the University of California.
 * Copyright (c) 1994 Sun Microsystems, Inc.
 * Copyright 2020 Google LLC
 * Relicensed as GPL-2.0+ for U-Boot
 *
 * Modified from commit fae05bc at::
 * github.com/embeddedartistry/embedded-resources/tree/master/examples/libc/stdlib
 */

#include <linux/ctype.h>
#include <stdbool.h>
#include <stddef.h>

/*
 * Largest possible base--10 exponent.  Any exponent larger than this will
 * already produce underflow or overflow, so there's no need to worry about
 * additional digits.
 */
static int maxExponent = 511;

/*
 * Table giving binary powers of 10.  Entry is 10^2^i.  Used to convert decimal
 * exponents into floating-point numbers
 */
static double powersOf10[] = {
    1.0e4,
    1.0e8,
    1.0e16,
    1.0e32,
    1.0e64,
    1.0e128,
    1.0e256
};

/*
 * Details on @str:
 */
double strtod(const char *string, char **endptr)
{
	int sign, expsign = false;
	double fraction, dblexp, *d;
	const char * p;
	int c;
	int exp = 0; /* Exponent read from "EX" field */
	/*
	 * Exponent that derives from the fractional part.  Under normal
	 * circumstatnces, it is the negative of the number of digits in F.
	 * However if I is very long, the last digits of I get dropped
	 * (otherwise a long I with a large negative exponent could cause an
	 * unnecessary overflow on I alone).  In this case, fracexp is
	 * incremented one for each dropped digit.
	 */
	int fracexp = 0;
	int mantsize; /* Number of digits in mantissa */
	int decpt; /* Number of mantissa digits BEFORE decimal point */
	/* Temporarily holds location of exponent in string */
	const char* pexp;

	/* Strip off leading blanks and check for a sign */
	p = string;
	while (isspace(*p))
		p += 1;
	if (*p == '-') {
		sign = true;
		p += 1;
	} else {
		if (*p == '+')
			p += 1;
		sign = false;
	}

	/*
	 * Count the number of digits in the mantissa (including the decimal
	 * point), and also locate the decimal point.
	 */
	decpt = -1;
	for (mantsize = 0;; mantsize += 1) {
		c = *p;
		if (!isdigit(c)) {
			if (c != '.' || decpt >= 0)
				break;
			decpt = mantsize;
		}
		p += 1;
	}

	/*
	 * Now suck up the digits in the mantissa.  Use two integers to
	 * collect 9 digits each (this is faster than using floating-point).
	 * If the mantissa has more than 18 digits, ignore the extras, since
	 * they can't affect the value anyway.
	 */
	pexp = p;
	p -= mantsize;
	if (decpt < 0)
		decpt = mantsize;
	else
		mantsize -= 1; /* One of the digits was the point */
	if (mantsize > 18) {
		fracexp = decpt - 18;
		mantsize = 18;
	} else {
		fracexp = decpt - mantsize;
	}
	if (!mantsize) {
		fraction = 0.0;
		p = string;
		goto done;
	} else {
		int frac1, frac2;

		frac1 = 0;
		for (; mantsize > 9; mantsize -= 1) {
			c = *p;
			p += 1;
			if (c == '.') {
				c = *p;
				p += 1;
			}
			frac1 = 10 * frac1 + (c - '0');
		}
		frac2 = 0;
		for (; mantsize > 0; mantsize -= 1) {
			c = *p;
			p += 1;
			if (c == '.') {
				c = *p;
				p += 1;
			}
			frac2 = 10 * frac2 + (c - '0');
		}
		fraction = (1.0e9 * frac1) + frac2;
	}

	/* Skim off the exponent */
	p = pexp;
	if (*p == 'E' || *p == 'e') {
		p += 1;
		if (*p == '-') {
			expsign = true;
			p += 1;
		} else {
			if (*p == '+')
				p += 1;
			expsign = false;
		}
		if (!isdigit(*p)) {
			p = pexp;
			goto done;
		}
		while(isdigit(*p)) {
			exp = exp * 10 + (*p - '0');
			p += 1;
		}
	}
	if (expsign)
		exp = fracexp - exp;
	else
		exp = fracexp + exp;

	/*
	 * Generate a floating-point number that represents the exponent.
	 * Do this by processing the exponent one bit at a time to combine
	 * many powers of 2 of 10. Then combine the exponent with the
	 * fraction.
	 */
	if (exp < 0) {
		expsign = true;
		exp = -exp;
	} else {
		expsign = false;
	}
	if (exp > maxExponent)
		exp = maxExponent;  /* errno = ERANGE; */
	dblexp = 1.0;
	for (d = powersOf10; exp != 0; exp >>= 1, d += 1) {
		if (exp & 01)
			dblexp *= *d;
	}
	if (expsign)
		fraction /= dblexp;
	else
		fraction *= dblexp;

done:
	if (endptr)
		*endptr = (char*)p;

	if (sign)
		return -fraction;

	return fraction;
}

double atof(const char *str)
{
	return strtod(str, NULL);
}
