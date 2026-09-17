---
schema: "implement-rfc.card.v1"
id: "BPR-0852"
title: "A Git cache connectivity refusal carries an empty diagnostic"
status: "In Progress"
rfcs: ["RFC-BPR-0029"]
depends_on: []
priority: "P3"
platforms: ["mac", "linux", "win"]
metadata: {"found_by":"dsp-004 BPR-0846 critical review, non-blocking finding 4","found_at":"bbdf8cc0e9d5db0ebd66b05b8f3d1fcd76a62a42","run":"run-20260916-stations","dispatch":"dsp-004"}
---

# BPR-0852: A Git cache connectivity refusal carries an empty diagnostic

## Defect

`verify_connectivity` (`src/adapters/git/src/installed_git_process.cpp`
~4747-4790) runs `git fsck --connectivity-only`. When closure is broken,
`fsck` prints `broken link` and `missing <type> <oid>` lines on **stdout**,
exits 2 and leaves stderr empty. The runner turns the exit into
`nonzero_exit` with `redacted_diagnostic(standardError)` before stdout is
looked at, so the refusal's detail is always empty. The BPR-0846 courses show
`EPOCH_CACHE_FAILURE:10:` and `UNCONNECTED_CACHE_FAILURE:10:` with nothing
after the code.

This existed before [[BPR-0846]]. That change makes it the main way a trimmed
pack over broken history is refused.

## Scope

- Carry a bounded, redacted summary of `fsck`'s stdout into the refusal
  detail on a non-zero exit, for example the kinds of missing object and
  their count. Keep the existing output bounds, and keep the success rule
  that stdout must be empty.

## Acceptance

- The unconnected BPR-0846 course's refusal names a missing-object kind, and
  a contract row pins the bound.

## Fix (dsp-004, Mac)

**What changed.** `verify_connectivity` passes
`summarize_connectivity_output` to the process runner, through a new
`process_request` field. On a non-zero exit, the runner appends a summary of
fsck's stdout to the redacted stderr text, within the stderr limit.

The summary counts three kinds of line:

- `missing <commit|tree|blob|tag>`;
- `broken link`;
- anything else.

An example is `fsck reported 1 missing commit, 1 missing blob, 2 broken links`.
The summary never repeats an object name, a path or unrecognised text. The
existing 1024-byte stdout limit still applies, and larger output is refused as
`standard_output_limit` with no text. The success rule, that stdout must be
empty, is unchanged.

**Evidence.**

- `git_haves_trimmed_pack_unconnected` prints
  `UNCONNECTED_CACHE_FAILURE:10:fsck reported 1 missing commit, 1 missing blob, 2 broken links`.
  It requires "1 missing commit", and it requires that the object's name does
  not appear.
- Process-contract rows pin these cases:
  - the exact summary, with no canary text;
  - 1024 bytes of output, still counted;
  - 1025 bytes, refused as `standard_output_limit` with no text.

  The real-Git missing-parent row requires "1 missing commit".
- Negative control: without the summary, both assertions fail
  (`build/r2-logs/dsp4-0851-negative-control.log`).

**Still open.** The critical review, which covers BPR-0851's slice, and the
Linux and Windows runs.
