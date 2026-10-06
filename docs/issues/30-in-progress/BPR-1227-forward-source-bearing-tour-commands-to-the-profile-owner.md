---
schema: "implement-rfc.card.v1"
id: "BPR-1227"
title: "Forward source-bearing tour commands to the profile owner"
status: "In Progress"
rfcs: ["RFC-BPR-0045"]
depends_on: ["BPR-1062"]
metadata: {"pack":"bpr-pr-tour-cli-extension-v1","origin":"discovered while accepting BPR-1061 and BPR-1062: with a desktop owner, the CLI forwards local edit commands but answers interaction_required to every command that reads the provider","reservation":"docs/issues/_reservations/BPR-1227--BPR-1227--3f02ead5aa45.json"}
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
