---
schema: "implement-rfc.card.v1"
id: "BPR-2206"
title: "application_profile_management_public_path stops at its Dif catalog check"
status: "Ready"
rfcs: []
metadata: {"reservation":"docs/issues/_reservations/BPR-2206--BPR-2207--fa4afcace75b.json","found_by":"BPR-2198"}
depends_on: []
---

# BPR-2206: the profile management course finds no Dif

## Scope and acceptance

`application_profile_management_public_path` fails on macOS (dev preset)
before any profile gesture:

```
FAILED: the desktop composes one complete accepted Dif catalog
```

At that point `initial->window().dif()` is null and the status reports 0
decoded clips and 0 load attempts; waiting up to 20 s changes nothing, so
it is not a race. Set aside, the later `work->window().dif()->cache_identity()`
dereferences null and crashes (EXC_BAD_ACCESS in
`shared_ptr<qt::dif_cache const>::get`).

It is **pre-existing**: it fails identically at `6de00c5b1`, the commit
before BPR-2198 (`304a77a08`), built in its own worktree. With its four Dif
checks set aside the rest of the course, BPR-2198's migrated Settings ›
Profiles steps included, passes. `application_profile_multi_window_public_path`
asserts the same 10 clips and passes, so the difference is in how this
course's desktop is composed (it starts through `start_desktop` with only
`ApplicationDataRoot` set).

Acceptance: find why this course's window has no Dif catalog, fix the
composition or the course (not by deleting the checks), and the course
passes whole on macOS.
