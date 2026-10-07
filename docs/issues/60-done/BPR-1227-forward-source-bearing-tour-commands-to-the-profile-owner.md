---
schema: "implement-rfc.card.v1"
id: "BPR-1227"
title: "Forward source-bearing tour commands to the profile owner"
status: "Done"
rfcs: ["RFC-BPR-0045"]
depends_on: ["BPR-1062"]
metadata: {"pack":"bpr-pr-tour-cli-extension-v1","origin":"discovered while accepting BPR-1061 and BPR-1062: with a desktop owner, the CLI forwards local edit commands but answers interaction_required to every command that reads the provider","reservation":"docs/issues/_reservations/BPR-1227--BPR-1227--3f02ead5aa45.json"}
acceptance: [{"criterion":"AC45-05","outcome":"pass","revision":"03deabbe7","evidence":{"kind":"command","path":"tests/end_to_end/profile_tour_handoff_contract.py","detail":"Real desktop owner with the fixture pull request open: shipped `bpr tour add review.cpp:3-5` commits through the owner and binds the same commit, blob, path, side and lines as the author's own gesture; its source text equals the desktop comparison pane. bpr-main-win windows-dev Debug, application_profile_tour_handoff_public_path. Control: desktop skipping the window read fails with interaction_required."}},{"criterion":"stop-equals-standalone","outcome":"pass","revision":"03deabbe7","evidence":{"kind":"command","path":"tests/application/tour_cli_tests.cpp","detail":"forwarded_source: standalone and forwarded add of one range give equal stops (all fields but the per-session stop ID; comparison digests are per session in both paths), durable after the owner exits; the forwarded add makes no provider read in the CLI process. tour_cli_composition. Control: CLI refusing forwarded source fails 'a forwarded add commits through the owner'."}},{"criterion":"refusals-without-partial-draft","outcome":"pass","revision":"03deabbe7","evidence":{"kind":"command","path":"tests/application/tour_cli_tests.cpp","detail":"Wrong-profile (another profile's draft on the channel), oversized (CLI and owner both refuse; control without the owner check fails), unpinned (short commit), unavailable (missing file; out-of-range on the real owner) and no-open-window reads refuse with revision unchanged and no request record, in tour_cli_composition and the real-owner course."}},{"criterion":"read-is-not-a-mutation","outcome":"pass","revision":"03deabbe7","evidence":{"kind":"command","path":"tests/sqlite_contract/tour_owner_channel_tests.cpp","detail":"A source request cannot carry a request ID (codec refuses; control admitting one fails sqlite_tour_owner_channel); show --source through the owner leaves the revision unchanged; only the forwarded add itself is recorded."}},{"criterion":"fixtures-and-safe-target","outcome":"pass","revision":"03deabbe7","evidence":{"kind":"command","path":"docs/verification/RFC-BPR-0044-0046.md","detail":"All evidence uses fixtures; no remote mutation. No live-provider run was made; any is for MikeMarcin/BPR-Test. macOS and Linux runs owed."}}]
---

# BPR-1227: Forward source-bearing tour commands to the profile owner

## Observation

The owner channel of [[BPR-1061]] and the live-draft routing of [[BPR-1062]]
carry `load`, `mutate` and `request_status` for local draft commands: status,
show, edit, move, remove, ready, undo and redo work against a running desktop.
`add`, `open`, `show --source`, `walk`, `preview --refresh`, `publish` and
`recover` answer `interaction_required` while a desktop owns the profile,
because they read the provider through profile-owned stores (epoch catalog, git
cache, device bindings) that only the owner may open.

RFC-BPR-0044's everyday flow includes `bpr tour add PATH:LINES`, and
AC45-05 says CLI commands work against a running desktop. Adding a stop from
source with the desktop open is therefore not yet possible.

## Outcome

Add a finite, versioned `source` operation to the tour channel. The owner
resolves the pinned destination through its own provider composition on its
serialized lane and returns the bounded source material; the CLI builds the same
stop it builds standalone and forwards it as an ordinary mutation. Provider reads
never reach the CLI process and no second process opens a profile store.

`open` for a pull request the owner has not composed, `preview --refresh`,
`publish` and `recover` remain separate command classes: publication needs live
authorization in the invoking presentation and stays `interaction_required`
through this channel.

## Direct acceptance

The shipped CLI adds a stop from exact source through a real desktop owner and
the stop equals the standalone result. Wrong-profile, oversized, unpinned and
unavailable-source requests refuse without a partial draft. A provider read is
never a mutation and adds no request record. Real provider evidence stays with
the designated safe target; fixtures cover the rest.

## Implementation (bpr-main-win, 2026-10-06)

**What changed.** The tour owner channel gains a finite `source` read; the owner
protocol goes from v5 to v6, and either side refuses the other version. A read
names the draft and one pinned commit, path, side and line range, and nothing
else. It carries no request ID, because a read records nothing.

- The desktop's profile route sends a read to the window whose current tour is
  that draft. The window reads the range through its own epoch host and Git cache
  on its diff lane (`review_tour_source_context`, then
  `materialize_review_tour_source`, the same pair a standalone read uses) and
  answers there. A closing window stops its reads and waits for none to be in
  flight.
- A draft no window shows goes to `tour_owner_service`, which refuses
  `interaction_required` without reading anything. An owner with no source reader
  (the test owners) refuses the same way.
- The CLI's `source` forwards instead of refusing. `add` builds the stop from the
  owner's material exactly as it does standalone and forwards it as an ordinary
  mutation, so only the stop is recorded. `show --source` and `walk` read the same
  way. Refusals read and exit as the standalone ones do.
- Assist takes no source evidence from a running desktop: `--sources` refuses
  before any read, and the comment hook's stop source is read standalone only.
- `capabilities` names owner protocol v6. The pr-tour skill's
  `interaction_required` row now says which commands read source through the
  desktop.

**Evidence** (bpr-main-win, `windows-dev`, `BPR_BUILD_TOUR_CLI=ON`, Debug). These
six pass together: `sqlite_tour_owner_channel`, `tour_cli_composition`,
`tour_cli_process_commands`, `tour_cli_public_entry`, `skill_pr_tour_package` and
`application_profile_tour_handoff_public_path`.

- **Real owner.** With the fixture pull request open in a real desktop, the
  shipped `bpr tour add review.cpp:3-5` binds the same commit, blob, path, side
  and lines as the author's own gesture on those lines, and its source text
  equals the desktop's comparison pane. Missing-file, out-of-range, unpinned and
  oversized adds refuse with no request record and no change.
- **CLI course.** Standalone and forwarded adds of one range give equal stops.
  The forwarded one makes no provider read in the CLI process. `show --source`
  leaves the revision unchanged. An owner with no window refuses. Oversized and
  other-profile reads sent on the channel directly refuse without reaching the
  reader.
- **Codec.** Source requests and answers round-trip; every malformed form
  refuses.

**Failing-before controls**, each built and run:

| Mutation | Fails |
|---|---|
| CLI refuses forwarded source again | `tour_cli_composition`: "a forwarded add commits through the owner" |
| Desktop skips the window read | the real-owner course, with `interaction_required` |
| Owner service drops its own size check | "the owner refuses an oversized read itself" |
| Codec admits a request ID on a read | `sqlite_tour_owner_channel` |

The first try of the CLI control did not compile: the early return left
unreachable code, which MSVC treats as an error. It was rerun as a conditional
return. The restored tree passes all six again.

**Limits.**

- A stop's comparison digest is per application session in both paths (epoch
  identities are random), so equality is over commit, blob, path, side, lines and
  text, not the digest.
- A window closing during a read is covered by construction (stop token plus an
  in-flight count that shutdown waits on), not by a course.
- No live-provider run; fixtures only. Any real-provider course belongs on
  `MikeMarcin/BPR-Test`.
- `open`, refresh, publish and recover stay `interaction_required` with a
  desktop owner, as scoped.
- Peers on protocol 5 now get `owner_protocol_unsupported`. RC58's
  owner-handoff course (BPR-1191) runs at protocol 6 from here.
- macOS and Linux runs of the six rows are owed.

Round-trip pins resealed (`github_application.cpp`, `tour_cli.cpp`,
`tour_runtime.*`, `tour_owner_service.*`); `PIN_CHECK_OK`. clang-format is clean
on every changed C++ file.

**After rebasing onto `1c652c97c`** (BPR-1067, BPR-1191 and BPR-1192 were
finished meanwhile), more places named the old protocol and were brought to v6:

- `docs/development/tour-cli.md`, which now describes the forwarded source
  reads.
- `bpr cycle`'s capabilities string and `docs/development/cycle-cli.md`, kept
  together by `cycle_cli_documentation`.
- The threat model's owner-endpoint entry. It now records the one source read
  the channel carries and its bound. The read grants nothing the same user's
  standalone CLI could not read itself.

The pins still hold (`cycle_cli.cpp` is outside the pinned digests). After a
full rebuild of the rebased tree, these 11 rows pass:

- the six above;
- `application_profile_cycle_handoff_public_path`;
- `application_review_cycle_security_qualification` (now protocol 6);
- `cycle_cli_public_path` and `cycle_cli_documentation`;
- `application_review_round_trip_composition_contract`.

**Over [[BPR-1088]]** (`707d2a04a`), which resealed the same pins for its own
tests-on change to `github_application.cpp`: the pin file keeps BPR-1088's
note, with this card's reseal note after it. The digests were recomputed over
the merged code (`PIN_CHECK_OK`). The merged tree rebuilt with no `warning C`
line, and the same 11 rows pass 11 of 11. The acceptance revision is the
rebased implementation commit `03deabbe7` (the push gate rebased once more).


**Correction (2026-10-06): the pr-tour skill wording is reverted.**

The edit to `packaging/agent-skills/pr-tour/references/recovery.md` is
undone. That package must equal the canonical AgentSkills copy MASK deploys,
and `skill_pr_tour_package` failed on the difference once `mask repair` had
deployed `pr-tour` on this station. That row passed earlier only because no
copy was deployed here.

The deployed row still says provider reads need the profile to themselves.
That is now conservative but not wrong in effect: an agent sees
`interaction_required` only when a read is refused.

Bringing the new wording over needs the change imported into AgentSkills'
`main` and the MASK selection refreshed. That is a push to another repository,
which needs its owner's authorization. It is left as a follow-up; no card
exists for it yet.

## macOS run (coordinator, 2026-10-06)

At 122e5d6b7 on macOS arm64 Debug with BPR_BUILD_TOUR_CLI=ON, clean worktree:
22/22 of the tour owner channel, tour CLI composition/process/public entry, both
skill package rows, both handoff courses, cycle security qualification, cycle
CLI public path and documentation, the round-trip contract, the settings window
and workbench settings rows, workbench settings storage, the feedback label rows
and architecture_boundaries.
