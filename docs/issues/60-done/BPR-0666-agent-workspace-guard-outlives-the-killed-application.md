---
schema: "implement-rfc.card.v1"
id: "BPR-0666"
title: "Agent workspace guard outlives the killed application"
status: "Done"
rfcs: []
depends_on: []
priority: "P2"
acceptance: [{"criterion":"agent_parent_death passes twenty consecutive runs on macOS","outcome":"pass","revision":"49566de8","evidence":{"kind":"command","detail":"ctest --test-dir build/r2-dev -R '^agent_parent_death$' --repeat until-fail:20: 20/20 pass on tree byte-identical to 49566de8; build/r2-logs/bpr0666-final-20.log"}},{"criterion":"The reason it was failing is named in the card's notes","outcome":"pass","revision":"49566de8","evidence":{"kind":"observation","detail":"30 instrumented kill courses: guard released 0.36-1.8 ms after supervisor raise via descriptor close at supervisor exit, never while paused; still held at first both-gone sample in 28/30; the check raced a prompt release and now has a bounded 5 s wait"}},{"criterion":"Clear the eleven strict-tidy errors in installed_git_process.cpp","outcome":"pass","revision":"49566de8","evidence":{"kind":"command","detail":"_coding-standard/scripts/tidy.py --warnings-as-errors on the file: 11 errors before, 0 after; no behaviour change; git_process_contract and agent_workspace_ownership pass"}}]
---

# Agent workspace guard outlives the killed application

`agent_parent_death` fails intermittently on macOS: two of three runs at
9ec5eba3 failed with "finished supervisor retained its workspace guard",
the third passed. The course kills the application, waits for the agent and
its descendant to stop, and then requires the workspace guard to be
available. The guard is still held when the check runs.

Either the supervisor releases the guard after the processes it supervises
have exited, in which case the product holds a lock nobody owns for an
unbounded moment, or the release is prompt and the check simply has no wait
where every other condition in the course has one. The course polls for the
processes to stop, so the distinction is visible: instrument the release
and see whether it lands within the same tick.

The failure predates RFC-BPR-0028 and belongs to no card in that pack; it
surfaced in the full matrix run at the pack's closeout head.

## Scope

- Decide which of the two it is before changing anything, by observing the
  release against the process exits.
- If the guard is released late, release it where the supervisor learns the
  processes are gone; if the check races a prompt release, give it the same
  bounded wait the surrounding conditions have and say what it waits for.
- Owned paths: the Unix agent supervisor and its workspace guard,
  `tests/scripts/agent_parent_death_tests.py`.

## Also in this file

`src/adapters/git/src/installed_git_process.cpp` carries eleven strict-tidy
errors that predate this card: one signed operand in a bitwise expression at
the guard's own `fstat` check, three more of the same, and the rest naming
style on parameters and a type alias. Nothing runs strict tidy over that
translation unit today, so they accumulated. Clear them with the guard fix,
since both are in the same file.

## Acceptance

- `agent_parent_death` passes twenty consecutive runs on macOS, and the
  reason it was failing is named in the card's notes.

## Notes and evidence, 2026-09-17 (run-20260916-stations, dispatch dsp-001-r2)

**Why it failed: the check raced a prompt release.** This is not a product
defect. The supervisor was instrumented with timestamps on the same clock as
a fixture sampling at 0.2 ms, over 30 kill courses:

- The supervisor saw the owner close, killed the group and reaped the agent
  within 0.25–1.9 ms. It raised its signal 0.01–0.8 ms after the reap.
- The guard came free 0.36–1.8 ms after that raise. The shared lock is released
  when the supervisor's own exit closes descriptor 4. It never came free while
  the supervisor was paused (30/30).
- The agent never looked gone before the supervisor reaped it (0/30):
  `kill(pid, 0)` still succeeds on an unreaped process.
- At the first sample where both processes were gone, the guard was still
  held in 28 of 30 courses.

The PID disappears at the reap, and the lock can only follow about a
millisecond later. The old check ran straight after a 20 ms process poll and
sometimes landed in that gap. Closing the descriptor earlier in the supervisor
would only narrow the gap, so the fix is in the course.

This card's premise was checked against [[BPR-0622]]: `0427dc76` is an
ancestor of `9ec5eba3`, so the original 2-of-3 failures already included that
fix. The supervisor and course are unchanged from `9ec5eba3` to HEAD. The
guard failure hit 2 of 25 runs at HEAD, with and without load.

**Fix** (`49566de8`). The guard check gets the same bounded wait as the process
check (5 s, polling every 20 ms), with a comment saying what it waits for. The
check that the guard stays held while the owner is dead and the supervisor is
paused is unchanged. The same commit clears the eleven strict-tidy errors in
`installed_git_process.cpp` with no behaviour change:

- three `static_cast<mode_t>` wraps on signed bitwise operands;
- the Glaze alias `T` renamed to `self`;
- six lambda parameters prefixed;
- a `NOLINTNEXTLINE` on `validate_trailing_whitespace`, the option Glaze finds
  by its library-defined name. Renaming it would silently disable the check.

The card's tally was slightly off: the errors are 3 signed-bitwise errors, 6
parameters, 1 alias and 1 member.

| Check | Result | Log |
|---|---|---|
| `ctest --test-dir build/r2-dev -R '^agent_parent_death$' --repeat until-fail:20` with the fix | **20/20 pass** | `build/r2-logs/bpr0666-final-20.log` |
| Same at HEAD before the fix | failed (run 5 under load; 18/20 independent runs) with `finished supervisor retained its workspace guard` | `bpr0666-repro.log` |
| Strict tidy on the file (`_coding-standard/scripts/tidy.py --build-dir build/r2-dev --warnings-as-errors`) | 11 errors → 0 | `bpr0666-tidy-before.log`, `bpr0666-tidy-after.log` |
| `git_process_contract`, `agent_workspace_ownership`, `application_agent_materialization_crash_recovery` | pass (87.8 s, 3.0 s, 44.8 s) | `bpr0666-neighbours.log` |
| clang-format dry run and `git diff --check` | clean | `bpr0666-format-diffcheck.log` |

The final batch ran against the working tree of `94f4e6dc` plus this change,
which is byte-identical to `49566de8`.

**Neighbour failures that are not this card's:**

- `architecture_boundaries` stops on three pre-existing findings, now
  [[BPR-0807]].
- `application_agent_crash_recovery` fails its live `startup-recover` step 3
  of 3 on macOS, with or without the tidy edit. That is [[BPR-0808]].
- A separate intermittent flake, where a plain canary launch does not end in
  success in about 1 of 60 launches, is [[BPR-0809]]. It can still break a
  future 20-run batch.

`agent_materialization_kill_recovery_tests.py` also waits for the
supervisor's PID and then relies on the lock. It may have the same gap, but it
was not seen failing. Linux timing was not observed.
