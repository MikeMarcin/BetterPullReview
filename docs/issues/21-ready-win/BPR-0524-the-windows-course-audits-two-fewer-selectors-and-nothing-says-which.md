---
id: BPR-0524
title: The Windows platform course audits two fewer selectors than macOS and Linux, and nothing says which
status: Ready
rfc: RFC-BPR-0016
depends_on: []
---

# The Windows platform course audits two fewer selectors than macOS and Linux, and nothing says which

## Why

`COURSE_SELECTOR_PATTERN` in `scripts/verify_rfc0016_platform_course.py` is
platform-independent and enumerates sixteen names (seventeen since
[[BPR-0523]] added `composed_application_hosts`). The recorded rows agree
on macOS and Linux (`RFC0016_COURSE_SELECTORS:16` on [[BPR-0318]],
[[BPR-0310]], [[BPR-0516]]) and disagree on Windows (`:14` on
[[BPR-0311]] and [[BPR-0511]]). `check_course_selectors` requires 100 %,
zero failures, and a total above zero; the total is printed and never
compared, so the Windows course passes while auditing two fewer things and
no output says which two. The pattern's comment promises that "a name that
is not registered cannot be quietly skipped", which a positive-count check
does not deliver. Noticed by `bpr-linux` on 2026-09-02 while [[BPR-0523]]
was editing that function.

## Required final state

- The two selectors absent from the Windows Debug tree are named, with the
  registration condition that excludes them and whether that exclusion is
  legitimate.
- The course compares the registered set against the pattern's enumerated
  names: a name missing on a platform is a refusal by name unless a
  documented per-platform exclusion table lists it with its reason, and
  the exclusion table is the only way a platform audits fewer names.
- Every platform's recorded count is explained by the pattern and the
  table alone.

## Verification

On the Windows station first:

```text
ctest --test-dir build\windows-dev -C Debug -N -R "<COURSE_SELECTOR_PATTERN>"
```

listed against the sixteen names the pattern enumerates; then the course's
own contract (`rfc0016_platform_course_tests.py`) gains cases for a missing
name and for a documented exclusion, and each platform course passes with
its count explained.
