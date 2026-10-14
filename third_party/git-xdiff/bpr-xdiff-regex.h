/*
 * [[BPR-1485]]. The regular-expression types Git's xdiff names in its public
 * header, for a build without Git's compatibility layer. BetterPullReview
 * never passes xdiff a regular expression (`ignore_regex` is always null), so
 * the types stay incomplete and no regex library is linked.
 *
 * Part of the corresponding source of the bpr_xdiff shared library, and
 * licensed like it: LGPL-2.1-or-later (see LGPL-2.1 beside this file).
 */
#ifndef BPR_XDIFF_REGEX_H
#define BPR_XDIFF_REGEX_H

typedef struct bpr_xdiff_unused_regex regex_t;

typedef struct bpr_xdiff_regmatch
{
	long rm_so;
	long rm_eo;
} regmatch_t;

#endif
