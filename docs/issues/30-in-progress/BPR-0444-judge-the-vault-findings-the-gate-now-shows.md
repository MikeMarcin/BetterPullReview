---
id: BPR-0444
title: Judge the nine vault findings the working tidy gate now shows
status: In Progress
rfc: RFC-BPR-0002
depends_on: [BPR-0443]
---

# Judge the nine vault findings the working tidy gate now shows

[[BPR-0443]] made `coding-tidy` run on the Linux-only sources for the first
time and resolved every identifier-naming finding. Nine non-naming findings
remain in `src/adapters/github/src/linux_secret_service.cpp`. They are older
than that card and were invisible while the gate could not execute.

| Count | Check | Where |
|---|---|---|
| 8 | `bugprone-suspicious-stringview-data-usage` | 394, 395, 618, 630, 713, 715, 820, 837 |
| 1 | `bugprone-branch-clone` | 1493 |

## Why they are not obviously nothing

Each `.data()` call hands a `std::string_view` to a libsecret C entry point
that expects a null-terminated string — `make_schema`, `secret_value_new`'s
content type, `secret_item_create_sync`'s label. On inspection the arguments
are `constexpr` views over string literals, which are null-terminated, so the
calls are very likely correct today. The check is warning about the shape
rather than a proven defect.

That is exactly why it needs a decision rather than a silent suppression. This
is the credential vault. A view that stops being a literal later would pass
review unchanged and read past its end into a secret-bearing call, and nothing
in the current code says the argument must stay a literal.

`bugprone-branch-clone` is the mildest: `SECRET_ERROR_ALREADY_EXISTS` and
`default` both return `failure::operation_failed`. Naming the case explicitly
next to a `default` that agrees is a legitimate style, and the honest outcome
here may be an accepted suppression with that reason recorded.

## Required final state

- Each of the nine is repaired, or suppressed with a written reason naming why
  the shape is safe and what would make it unsafe.
- No suppression is added without that reason.
- If any is a real defect, it gets its own card and a regression test rather
  than an inline fix folded into a hygiene pass.
- `coding-tidy` is clean on the Linux-only sources afterwards.

## Not this card

The identifier naming, which [[BPR-0443]] finished, and the native Secret
Service fixture failures recorded on [[BPR-0310]].

## Ready, and ready on Linux — 2026-08-23

[[BPR-0443]] is done, so nothing is queued ahead of this card. The lane is
Linux because the proof is: `linux_secret_service.cpp` appears only in a
Linux `dev` tree's compilation database — a macOS or Windows pass never
compiles it — so "coding-tidy is clean on the Linux-only sources afterwards"
can only be shown there. Judging the findings is portable reading, but the
split rule takes no card whose portable half is unverifiable here, so the
whole card moves to `23-ready-lin/`.
