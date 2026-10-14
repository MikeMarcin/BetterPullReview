/*
 * [[RFC-BPR-0082]] [[BPR-1466]]. The few git-compat-util.h facilities Git's
 * xdiff uses, so the evaluation can compile xdiff from the pinned Git source
 * without the rest of Git. First-party; evaluation builds only.
 */
#ifndef BPR_EVAL_XDIFF_GIT_COMPAT_UTIL_H
#define BPR_EVAL_XDIFF_GIT_COMPAT_UTIL_H

#include <ctype.h>
#include <limits.h>
#include <regex.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define UNUSED __attribute__((unused))

#define bitsizeof(x)                    (CHAR_BIT * sizeof(x))
#define maximum_signed_value_of_type(a) (INTMAX_MAX >> (bitsizeof(intmax_t) - bitsizeof(a)))
#define signed_add_overflows(a, b) \
	((b) > maximum_signed_value_of_type(a) - (a))

static inline void bpr_eval_xdiff_bug(const char* where)
{
	fprintf(stderr, "xdiff BUG: %s\n", where);
	abort();
}
#define BUG(...) bpr_eval_xdiff_bug(__FILE__)

static inline void* xmalloc(size_t size)
{
	void* result = malloc(size ? size : 1);
	if (!result) {
		abort();
	}
	return result;
}

static inline void* xcalloc(size_t count, size_t size)
{
	void* result = calloc(count ? count : 1, size ? size : 1);
	if (!result) {
		abort();
	}
	return result;
}

static inline void* xrealloc(void* pointer, size_t size)
{
	void* result = realloc(pointer, size ? size : 1);
	if (!result) {
		abort();
	}
	return result;
}

static inline int regexec_buf(const regex_t* preg, const char* buf, size_t size,
							  size_t nmatch, regmatch_t pmatch[], int eflags)
{
	pmatch[0].rm_so = 0;
	pmatch[0].rm_eo = (regoff_t)size;
	return regexec(preg, buf, nmatch, pmatch, eflags | REG_STARTEND);
}

#endif
