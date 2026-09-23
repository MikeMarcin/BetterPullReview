---
schema: "implement-rfc.card.v1"
id: "BPR-0876"
title: "Windows: six contained courses fail around the temporary helper, at a denied chmod and an exit-3 seed"
status: "In Progress"
rfcs: []
depends_on: []
priority: "P2"
platforms: ["win"]
metadata: {"found_by":"bpr-win task-20260917-win-merged-head-runs, verifying BPR-0854 item 1","found_at":"2b2e5549","run":"run-20260916-stations","dispatch":"dsp-005"}
---

# BPR-0876: Windows: six contained courses fail around the temporary helper, at a denied chmod and an exit-3 seed

## Defect

Six courses fail on the Windows station:

- `application_device_binding_relaunch` — `os.chmod` fails with WinError 5
- `application_profile_owner_admission_public_path` — "FAILED: seed registry"
- `application_profile_window_restart_public_path` — "seed failed (3)"
- `application_profile_window_layout_public_path` — "seed failed (3)"
- `application_profile_window_review_restoration_public_path` — "auth-seed failed (3)"
- `application_device_binding_launch_renewal` — "private desktop test root under the canonical storage policy"

**The contained temporary helper is not the cause, and that was measured
rather than assumed.** Inside the container the station showed that a root
made the helper's way (`os.mkdir`, no mode, under `tempfile.gettempdir()`) is
created; a child directory inside it is created; a file is written; and
`icacls` shows the container's own SID inherited `(I)(OI)(CI)(CR)(F)`. So the
contained driver **can** use the directory it just created, and
`make_storage_root_owner_private` has a root it can accept. That was the open
Windows question on [[BPR-0854]] item 1, and the answer is that the helper is
sound.

What actually fails is on either side of it:

- `os.chmod(<dir>, 0o700)` is **denied inside the container**
  (`tests/end_to_end/device_binding_relaunch_contract.py:103`).
- The seeding executables **exit 3**, this station's known contained abort for
  `std::filesystem::canonical` ([[BPR-0833]], [[BPR-0845]]).

**Neither was introduced by BPR-0854 item 1.** That commit changed only the
`with tempfile.TemporaryDirectory(...)` line to
`with contained_temporary_directory(...)`; the `os.chmod` two lines below came
in with `1925d0cc` ([[BPR-0644]]) and is untouched. Verified by
`git log -L` over that range. So item 1 did not regress Windows, and this card
is a pre-existing contained-environment problem that the run's first full
Windows sweep made visible.

[[BPR-0868]]'s leak was also seen here exactly once and unreported:
`AC\Temp\bpr-0644-relaunch-612768de307ca452`, left by the row that died at
`os.chmod`. The other four prefixes left nothing. That is a concrete instance
of the helper swallowing a removal error on Windows.

## Scope

- The `os.chmod(root, 0o700)` is a POSIX gesture with no meaning on Windows,
  where the directory's ACL already carries the container's entry. Make it
  conditional, or replace it with something that expresses the same intent on
  both platforms. Do not simply drop it: it is what the POSIX owner-only
  contract rests on, and [[BPR-0847]]/[[BPR-0849]] depend on that contract
  holding.
- The exit-3 seeds are the `std::filesystem::canonical` contained abort
  already recorded in BPR-0833 and BPR-0845. Check whether those cards' fix
  covers these call sites, and say so here rather than re-deriving it.
- Decide per course whether it is meant to run contained at all; see
  [[BPR-0872]], which asks the same question of eleven other rows and should
  probably be answered together with this one.

## Acceptance

- The six courses pass on Windows, or each carries a recorded, reasoned
  registration change.
- The POSIX owner-only parent contract still holds on macOS and Linux, with
  its negative controls still failing by name.
- This card records that BPR-0854 item 1 did not cause these failures, so the
  helper is not blamed for them later.

## Decomposed (dsp-006 scout): two causes, and only one is a containment question

The containment rule is recorded once on [[BPR-0872]]. All six rows are
contained today. They split cleanly:

- **One row is a test-code fix the station can make alone.**
  `tests/end_to_end/device_binding_relaunch_contract.py:103` calls
  `os.chmod(root, 0o700)`, a POSIX gesture with no Windows meaning.
  `scripts/contained_temporary_support.py:58-60`
  (`inherits_parent_permissions()`) is the existing and already-correct idiom
  for exactly this case, so the fix is local to the test file and needs no
  `scripts/**` change. The helper's Windows path is otherwise sound: it creates
  with no explicit mode, so the parent's `(I)(OI)(CI)(M)` container grant
  inherits.
- **Four exit-3 seed courses are a containment decision.** The cause is the
  `std::filesystem::canonical` contained abort — vocabulary `dos-volume-name`.
  [[BPR-0833]]'s integrator decision was explicit that *no* choice of parent
  passes contained while the stores require `validate_ancestor_chain`, and
  relaxing that walk is a product security change out of scope here. So pin
  them uncontained; the precedent is `application_composition_capabilities_*`.

**These do not block on [[BPR-0867]] or [[BPR-0868]].** 0867's `onexc` gap is
latent — every machine in this run exceeds 3.12 — and 0868 is a diagnostic
multiplier rather than a prerequisite: making the helper report a failed
removal by name would have surfaced this card's single leak
(`AC\Temp\bpr-0644-relaunch-...`) for free instead of by hand.
