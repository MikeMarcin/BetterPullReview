---
schema: "implement-rfc.card.v1"
id: "BPR-0868"
title: "The contained temporary helper swallows a removal error that tempfile would have raised"
status: "Backlog"
rfcs: []
depends_on: ["BPR-0876"]
priority: "P3"
platforms: ["win"]
metadata: {"found_by":"bpr-linux task-20260917-lnx-sweep-fixes review of BPR-0854 item 1","found_at":"e84eaba2","run":"run-20260916-stations","dispatch":"dsp-005"}
---

# BPR-0868: The contained temporary helper swallows a removal error that tempfile would have raised

## Defect

`scripts/contained_temporary_support.py` catches `OSError` when it removes its
directory. `tempfile.TemporaryDirectory`, which the courses used before, raises.
So moving a course onto the helper quietly narrows what a failed cleanup does:
what would have failed the row now passes silently.

**Where it actually is** (corrected in dsp-006; the original description was
looser than the code). It is not a blanket catch around the removal. It is
`except OSError: pass` at `:104-106`, inside `_force_writable` — the *retry*
handler `shutil.rmtree` calls back into. So the removal's first failure is
handled, a read-only bit is cleared, the removal is retried once, and it is only
the **retry's** failure that is dropped. The effect is the one this card
describes — no exception and no message — and the fix is the same, but the
narrower location is what a reader needs to find it.

**On Windows it can hide something real.** A directory held open by a lingering
handle — the ordinary Windows sharing case, and the case the contained support
exists for — would leak instead of failing the row. The course would pass and
leave the leak in `%TEMP%`.

**On POSIX it hides a failure too, which the original bound wrongly** (corrected
in dsp-006). The original claim was that "every course prints its `_OK` marker
before the directory is removed, so a removal error arrives after the verdict
either way", which would have made [[BPR-0890]] apply and left the raise
verdict-neutral. Measured instead of assumed, by parsing each course and asking
whether the marker's `print` is a descendant of the contained `with`: in every
contained course the marker is a *sibling* of the `with`, not a child, so the
directory is removed **before** the marker is printed. See the dsp-006 section
below for the full table. It remains true that no course asserts on removal.

[[BPR-0854]] item 1 moved thirteen call sites onto this helper, five of which
run inside the Windows AppContainer, so the Windows half of this is now
reachable by more courses than before. It did not create the behaviour.

## Scope

- Decide what a failed removal should mean, and make the helper say it. The
  likely answer is that it should surface: a temporary directory that cannot be
  removed on Windows is usually a handle the course leaked, which is worth
  knowing about and is exactly what a contained-temporary helper is for.
- If some caller genuinely needs tolerance, make that explicit at the call
  site rather than the default for every caller.
- Either way the helper should not be silent: at minimum report the path and
  the error, so a leak is visible in the run's output even if it does not fail
  the row.
- Check this against the Windows runs [[BPR-0854]] still owes for its five
  contained courses; if one of them is already leaking a directory, this card
  is how it would be noticed.

Settle alongside [[BPR-0867]], which is the same helper's interpreter-version
gap.

## Acceptance

- A removal error is either raised or reported by name, and the card records
  which and why.
- No caller depends on silent tolerance, or the ones that do say so at the call
  site.
- The five Windows contained courses from BPR-0854 item 1 run without leaking a
  directory, and a deliberately held handle is shown to be reported.

## Settled: raised *and* reported (dsp-006, macOS)

**Both, not either.** The two answer different questions and cost one line
together, so the helper does both. A removal the retry cannot fix prints

```
contained temporary: could not remove <path>: <error>
```

to standard error and then raises `RemovalFailed`, a subclass of `OSError` so
that a call site already treating a failed cleanup as an ordinary filesystem
failure keeps treating it as one
(`scripts/contained_temporary_support.py:176-206`).

**Why raise by default.** These call sites were migrated off
`tempfile.TemporaryDirectory`, which raises, so raising restores the contract
they already had rather than inventing one. Two behaviours were copied from
CPython's own `TemporaryDirectory._rmtree` rather than invented: a root that is
already gone is success, not a failure, and everything else surfaces.

**Why report as well.** The raise is not universally load-bearing and the report
is: a caller that catches broadly turns the raise into an unnamed `fail:internal`
(`tests/scripts/verify_native_https_transport.py:1609-1614` does exactly that),
and a row whose marker is already on standard output would swallow it under
[[BPR-0890]]. The stderr line survives both. A leak nobody can see was the
defect.

**No tolerance knob was added, because no caller needs one.** An unused option is
a second contract to keep true. A caller that later needs tolerance writes
`except RemovalFailed:` at its own call site, which is explicit in *that*
caller's source — which is what this card asked for, and strictly better than a
boolean the reader has to go and look up.

### The BPR-0890 question, checked rather than assumed

BPR-0890 says a crash after the marker passes any row with a
`PASS_REGULAR_EXPRESSION`. It does **not** shield these removals, because the
removal is not after the marker. Every `_OK` marker in the tree that shares a
file with a contained temporary was located by AST and classified by whether its
`print` is inside the contained `with`:

| Course | Marker | Removal vs marker | Row's `PASS_REGULAR_EXPRESSION` |
|---|---|---|---|
| `device_binding_relaunch_contract.py:120` | `DEVICE_BINDING_RELAUNCH_OK` | before | `application_device_binding_relaunch` |
| `profile_activation_contract.py:250` | `PROFILE_ACTIVATION_OK` | before | none (exit code) |
| `profile_owner_admission_contract.py:131` | `PROFILE_OWNER_ADMISSION_OK` | before | none (exit code) |
| `profile_window_restart_contract.py:28` | `PROFILE_WINDOW_RESTART_OK` | before | none (exit code) |
| `profile_window_layout_contract.py:76` | `PROFILE_WINDOW_LAYOUT_OK` | before | none (exit code) |
| `profile_window_review_restoration_contract.py:146` | `PROFILE_WINDOW_REVIEW_RESTORATION_OK` | before | none (exit code) |
| `epoch_ordinary_update_public_path.py:163` | `EPOCH_ORDINARY_RECREATED_OK:1` | before | `application_epoch_ordinary_update_recreated` |
| `epoch_portable_occurrence_public_path.py:319` | `EPOCH_PORTABLE_TWO_CATALOGUES_OK:3` | before | `application_epoch_portable_occurrence_two_catalogues` |
| `verify_epoch_history_fixture.py:277` | `EPOCH_HISTORY_FIXTURE_OK` | before | `epoch_history_fixture_contract` |
| `verify_profile_window_restoration_fixture.py:125` | `PROFILE_WINDOW_RESTORATION_FIXTURE_OK` | before | `profile_window_restoration_fixture_contract` |
| `agent_workspace_ownership_tests.py:396` | `AGENT_WORKSPACE_OWNERSHIP_OK` | before | none (exit code) |
| `large_review_fixture_tests.py:194` | `LARGE_REVIEW_FIXTURE_OK …` | before | none (exit code) |
| `recorded_pull_request_fixture_tests.py:347` | `RECORDED_FIXTURE_OK` | before | none (exit code) |
| `epoch_portable_occurrence_public_path.py:261` | `EPOCH_REWRITTEN_BASE_OK` | **after** | none (exit code) |

**So: a removal error now fails the row, everywhere it matters.** On a row that
passes by exit code, the non-zero exit fails it. On a row that passes by
`PASS_REGULAR_EXPRESSION`, the raise happens before the `print`, so the marker is
never emitted and the expression never matches — which fails it just as surely,
and without depending on BPR-0890 being fixed first. Exactly one marker in the
whole tree is printed inside a contained `with`
(`epoch_portable_occurrence_public_path.py:261`), and no `PASS_REGULAR_EXPRESSION`
row names it, so even that one fails by exit code. There is no case in the tree
today where BPR-0890 would hide a removal error; the stderr report is insurance
against the case rather than a response to an observed one.

Log: `build/r2-logs/dsp6-0868-marker-ordering-full.log`.

### No caller depended on the silent tolerance

All 56 first-party files that call the helper were scanned by AST, not sampled.
Five
use it inside a `try` whose handler would catch an escaping `OSError`; each was
read, and each **reports** rather than continues:

| Call site | Handler | What a removal error now does |
|---|---|---|
| `tests/github_contract/verify_native_git_smart_transport.py:646` | `(OSError, ValueError, SubprocessError, ProbeFailure)` | prints `FAILED: <error>`, returns 1 |
| `tests/scripts/verify_gh_cli_trust.py:825` | `(OSError, UnicodeError, SubprocessError, ProbeFailure)` | prints `FAILED: <error>`, returns 1 |
| `tests/scripts/verify_native_https_transport.py:1583` | `ProbeFailure`, then `Exception` | prints `<label>:fail:internal`, returns 1; the stderr report is what names the path |
| `tests/github_contract/verify_native_https_device_authorization.py:825` | `ProbeFailure`, then `Exception` | prints `<marker>:fail:internal:RemovalFailed`, returns 1 |
| `tests/scripts/verify_gh_cli_trust.py:850` | same block | creation only; no cleanup involved |

None of them swallows and continues, so none relies on tolerance and none
needed an opt-in. Two adjacent patterns were checked and cleared as well: files
that remove a tree themselves remove a *child* of the contained root
(`tests/scripts/dif_asset_tests.py:1162`) or an unrelated root outside it
(`tests/end_to_end/rfc0015_cli_read_fixture_tests.py:37`), so nothing removes the
same path twice; and where something is removed twice, the already-gone rule
makes it quiet. No file was edited outside this card's own two.

### Verification (dsp-006, macOS 15, CPython 3.14.7, no build required)

`tests/scripts/contained_temporary_support_tests.py`, 11 cases, 0.008 s, all
pass. Its BPR-0868 half asserts that the retry reports the path and raises, that
the retry still clears a read-only bit and retries, that an already-gone root is
quiet, and — end to end — that a removal the retry cannot fix escapes
`contained_temporary_directory` with the path named on stderr.

Negative controls, on mirrored copies:

| Injected | Fails |
|---|---|
| `except OSError: … return` restored in the retry | `test_the_retry_reports_the_path_and_raises`, `test_a_removal_the_retry_cannot_fix_escapes_the_context_manager` |
| raise kept, report removed | both of the above |

Re-run clean after the change: `contained_temporary_usage_contract` (6 cases,
1.72 s), `documentation_issue_graph_contract` (64 cases, 2.18 s),
`make_project_source_dumps_tests` (2), `allocate_issue_id_tests` (2),
`core_boundary_tests` (4), `conditional_branch_identifiers` (11),
`attended_review_verdict_tests` (`:pass`). Logs under `build/r2-logs/dsp6-*`.

### Acceptance (dsp-006)

- **Met.** Raised *and* reported by name; which and why is above.
- **Met.** No caller depends on silent tolerance — all 56 audited, five
  `try`-wrapped sites read individually, none continuing silently.
- **Not met here; it is the Windows station's.** The five contained courses were
  not run, and no handle was held open, because this station is macOS and no
  build directory was available to this slice. The deliberately-held-handle case
  is also the only way to exercise the retry's *rescue* under a real
  `FILE_ATTRIBUTE_READONLY`; on POSIX a read-only file in a writable directory is
  removed without any retry, so the gate proves the rescue at the seam instead
  and says so. [[BPR-0876]]'s recorded leak,
  `AC\Temp\bpr-0644-relaunch-612768de307ca452`, is the case to re-run first: with
  this change that row prints the path and raises instead of leaking quietly.

## Two of three acceptance lines are met; the third needs the Windows station (dsp-006)

Landed in `e9c9d5c6`, with [[BPR-0867]] finished alongside it. Verified on this
Mac in the dispatch's integration pass:
`contained_temporary_support_contract` Passed 0.09 s,
`contained_temporary_usage_contract` Passed 1.72 s,
`test_network_isolation_manifest` Passed 4.41 s
(`build/r2-logs/dsp6-int-newrows.log`).

- **Met** — a removal error is raised *and* reported by name, and the card
  records which and why.
- **Met** — all 56 first-party callers were audited by AST rather than sampled.
  Five catch an escaping `OSError`; every one reports and exits non-zero, so
  none depended on the silence. No tolerance knob was added, because no caller
  needs one.
- **Not met, and it cannot be met from here** — "the five Windows contained
  courses from [[BPR-0854]] item 1 run without leaking a directory, and a
  deliberately held handle is shown to be reported."

The card therefore stays In Progress. That last line is also the only way to
exercise the **retry's rescue** under a real `FILE_ATTRIBUTE_READONLY`: on
POSIX a read-only file inside a writable directory is removed with no retry at
all, so the Mac gate proves the seam and says so in its own docstring rather
than overclaiming. [[BPR-0876]]'s recorded leak
`AC\Temp\bpr-0644-relaunch-612768de307ca452` is the first case to re-run — that
row now prints the path and raises instead of leaking quietly.

Noted so nobody rediscovers it as a swallow: three scripts use
`shutil.rmtree(..., ignore_errors=True)`
(`scripts/run_rfc0016_production_course_attended.py:514`,
`scripts/verify_runtime_surface.py:298`, and one under
`scripts/verification/rfc0016_attended_contract/`). That is deliberate,
explicit tolerance stated at the call site, which is exactly the shape this
card asks for, so it was left alone.

## The Windows criterion is closed, on a real leak (dsp-006, `bpr-win`)

The acceptance line only the Windows station could close asked for "a
deliberately held handle ... shown to be reported". It was answered better than
that: **the report came from a real leak, not a planted one.**

Before [[BPR-0876]]'s `mkdir` guard landed, the device-binding relaunch course
was leaving a directory behind, and the helper named it instead of swallowing
it:

    scripts.contained_temporary_support.RemovalFailed: contained temporary:
    could not remove C:\Users\...\Packages\bprautomatedtest\AC\Temp\
    bpr-0644-relaunch-eedb824449576f0e\application: [WinError 5] Access is denied

That is exactly the behaviour this card existed to create: a leak that used to
be invisible now stops the row and names its path. No held handle was needed.

The leak's own cause was found and fixed in the same pass, and it took **two**
guards rather than one: guarding only `os.chmod` left the course failing at
removal, because `root.mkdir(mode=0o700)` stamps CPython's protected owner-only
descriptor on Windows — which **excludes the AppContainer the course runs in**,
so the course had created a directory it could neither narrow nor delete.

What remains is not this card's: the relaunch row now gets past both guards and
fails at a named startup refusal, `registry_unavailable`, which is the same
value [[BPR-0875]]'s benchmark measured and [[BPR-0856]]'s rows hit. That family
is tracked on [[BPR-0876]] and the station has named it rather than guessed at
it.

So all three acceptance lines are now met in substance. The card is left In
Progress only because "the five Windows contained courses run without leaking"
was demonstrated on one of the five; the other four root under the container
temp and wait on the same startup-refusal fix.

## Reconciled to Backlog by the Mac main station (2026-09-23, Mac batch 3)

Idle since 2026-09-17. The fix landed in `e9c9d5c6` and does not need to
change. On macOS at `7fc4f6ab`, `contained_temporary_support_contract` passes
(0.10 s) and so does `contained_temporary_usage_contract` (2.15 s)
(`build/mac3-stale-cards.log`). Two acceptance lines are met, and the
Windows-station section above shows the report working on a real leak.

What is left is one line and it is Windows-only: the five contained courses
from [[BPR-0854]] item 1 run without leaking a directory. One of the five
was shown. The other four still stop at their exit-3 seeds before they reach
removal, and [[BPR-0876]] (Ready, Windows) owns that. No Mac or Linux work
remains, so the card moves to Backlog with `platforms: ["win"]` and a
dependency on BPR-0876. It returns to Ready for `bpr-main-win` when BPR-0876
is Done. Then one Windows run of the five courses, with no leftover
`bpr-*` directory under the container's `%TEMP%`, closes it.
