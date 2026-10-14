/*
 * [[BPR-1485]]. The few git-compat-util.h facilities Git 2.56.0's xdiff uses,
 * so the bpr_xdiff shared library is built from the pinned Git source's
 * xdiff/ directory alone (cmake/BprXdiffProvider.cmake). It replaces Git's own
 * header for that build and nothing else.
 *
 * Unlike Git's, the allocators here return null on failure instead of ending
 * the process: xdiff checks every allocation and fails the call, and the
 * application then refuses the comparison. Regular expressions are never used
 * (see bpr-xdiff-regex.h).
 *
 * Part of the corresponding source of the bpr_xdiff shared library, and
 * licensed like it: LGPL-2.1-or-later (see LGPL-2.1 beside this file).
 */
#ifndef BPR_XDIFF_GIT_COMPAT_UTIL_H
#define BPR_XDIFF_GIT_COMPAT_UTIL_H

#include "bpr-xdiff-regex.h"

#include <ctype.h>
#include <limits.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#if defined(_MSC_VER)
#define UNUSED
#else
#define UNUSED __attribute__((unused))
#endif

#define bitsizeof(x)                    (CHAR_BIT * sizeof(x))
#define maximum_signed_value_of_type(a) (INTMAX_MAX >> (bitsizeof(intmax_t) - bitsizeof(a)))
#define signed_add_overflows(a, b)      ((b) > maximum_signed_value_of_type(a) - (a))

static inline void bpr_xdiff_bug(const char* where)
{
	fprintf(stderr, "xdiff BUG: %s\n", where);
	abort();
}
#define BUG(...) bpr_xdiff_bug(__FILE__)

static inline void* xmalloc(size_t size)
{
	return malloc(size ? size : 1);
}

static inline void* xcalloc(size_t count, size_t size)
{
	return calloc(count ? count : 1, size ? size : 1);
}

static inline void* xrealloc(void* pointer, size_t size)
{
	return realloc(pointer, size ? size : 1);
}

static inline int regexec_buf(const regex_t* preg, const char* buf, size_t size, size_t nmatch, regmatch_t pmatch[], int eflags)
{
	(void)preg;
	(void)buf;
	(void)size;
	(void)nmatch;
	(void)pmatch;
	(void)eflags;
	bpr_xdiff_bug("regexec_buf: BetterPullReview never gives xdiff a regular expression");
	return 1;
}

#endif
