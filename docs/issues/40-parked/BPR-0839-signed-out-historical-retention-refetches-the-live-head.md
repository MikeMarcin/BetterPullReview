---
schema: "implement-rfc.card.v1"
id: "BPR-0839"
title: "Signed-out historical retention refetches the live head with no haves"
status: "Parked"
rfcs: ["RFC-BPR-0029"]
depends_on: []
priority: "P3"
platforms: ["mac", "linux", "win"]
metadata: {"found_by":"dsp-003 BPR-0829 slice","found_at":"c1a795c3ed724eb0086aa4661082793f9643a679","run":"run-20260916-stations","dispatch":"dsp-003"}
parked_on: "human-review"
---

# BPR-0839: Signed-out historical retention refetches the live head with no haves

## Observation

While diagnosing [[BPR-0829]], the anonymous public preview fixture course
showed four smart-Git leases: the live epoch's pack, then one background
retention import per force-push occurrence on the timeline (events 91701,
91702 and 91703; `src/application/src/epoch_application_host.cpp`
`start_historical_import` ~6282, next import ~6503). Two of the three imports
fetch head `f9108913`, which is the live head already in the cache, and they
fetch with no haves (~6283). Each import took about 7.5 to 8 s in the fixture
at load 3 to 19 (`build/r2-logs/dsp3-0829-instrumented*.log`).

For a signed-out read this spends the shared unauthenticated budget and time
on bytes already present.

## Scope

- Confirm whether an occurrence whose after-head is already retained (or is
  the live head) needs an import at all, and whether imports should offer
  haves.
- If the import is skipped or narrowed, [[BPR-0829]]'s course pins
  `PUBLIC_PREVIEW_SMART_ACQUISITIONS:4` and must change with it, with the
  reason.

## Acceptance

- The card records whether the refetch is needed; if not, the import no
  longer refetches an already retained head, the course's count is updated
  with its reason, and the epoch persistence and portable occurrence courses
  still pass on macOS.

**Platform timing (dsp-003).** With [[BPR-0829]]'s wait in place, the whole
course takes about 1.5 s on Linux (`bpr-linux`, 5 runs) and 33 to 36 s on
macOS, where each retention import took about 7.5 s. Whether the macOS cost
is the refetch, the sandboxed Git process or the fixture transport is not
established.

## Current frontier (dsp-003, bpr-linux, task-20260917-lnx-epoch-and-startup)

- **Outcome:** parked on one design question (below). No product or course
  file changed. `PUBLIC_PREVIEW_SMART_ACQUISITIONS:4` stays as it is.
- **Revision:** `e3047a116ea68ad7d5591edb7a8f1b9dc713c8b5` on `linux-lane`,
  with the stock `dev` build.
- **Evidence:** the trace below. Logs are in
  `/home/mmarcin/bpr-station-logs/task-20260917-lnx-epoch-and-startup/`
  (`trace-0839-*`). None is committed. The throwaway instrumentation was
  reverted and the build rebuilt before any commit.
- **Next:** an answer to the question. Each option names its own course and
  document changes.

## Finding (dsp-003)

**The refetch adds no objects, but the only import path in the current
contract cannot run without it.**

### What an import of a resident head does (observed)

Three runs of `application_anonymous_public_preview_public_path` were traced
at this revision. The trace printed to stderr from `start_historical_import`
and from the live fetch; it was a throwaway and has been removed. All three
runs passed, with `OCCURRENCES:4` and `SMART_ACQUISITIONS:4`, and each run
printed the same sequence:

| Lease | Wants | Pack | Installed | Import time |
|---|---|---|---|---|
| Live epoch | live head `f9108913`, no haves | 1584 bytes | new pack | not traced |
| Occurrence with head `f9108913` (the live head) | `f9108913`, no haves | 1584 bytes | **byte-identical to the live pack** (`ExpectedPackPreexisting`), so nothing new | fetch done at 4 ms, installed at 56 to 58 ms, committed at 224 to 228 ms |
| Occurrence with head `6458d96b` (prior head) | `6458d96b`, no haves | 1617 bytes | new pack | fetch done at 4 to 5 ms, committed at 227 to 233 ms |
| Occurrence with head `f9108913` again | `f9108913`, no haves | 1584 bytes | byte-identical again | committed at 225 to 232 ms |

Each import's derived comparison base was `757e8f25`, the live base.

- **The two refetches of the live head are pure transfer.** Their packs are
  already installed.
- **On Linux the transfer is not the cost.** The in-process fixture fetch
  takes about 1 ms of an import of about 225 ms. Installation (index-pack,
  derived base, connectivity) takes about 55 ms, and the durable
  staging/ref/catalogue commit about 170 ms. That timing is from Linux only.
  The macOS figure of 7.5 s per import is therefore probably not the refetch,
  but no Mac run has shown where it goes.
- **Against a real server the cost is larger.** With no haves, the fetch of a
  head is its complete reachable closure, the same bytes as the first live
  fetch. The import cap (8), `OperationPackBytes` and the pack-count quota
  bound it.
- **No REST budget check was found on the lease path.** The anonymous lease
  factory (`github_application.cpp` ~1914) counts leases. The anonymous smart
  transport it builds (`native_https_transport.cpp` ~4492) checks only that
  the profile admits the anonymous read contract. So the "shared
  unauthenticated budget" named above is not shown to be spent by this lease.

### Why the fetch cannot simply be skipped (code)

- **The import needs a pack receipt.** A historical append reaches the cache
  only through `prepare_historical_revision` → `prepare_revision`
  (`git_cache_port.hpp` ~797). `valid(git_prepare_revision_request)` requires
  this operation's completed pack receipt, pre-import inventory and import
  authorization tokens (~247-270).
- **The journal records the fetch.** The append journal records receiving,
  spool and install facts that come from that fetch
  (`epoch_application_host.cpp` ~6256, ~6307, ~6347), and `recover_append`
  replays them.
- **No other path exists.** `git_cache_session` has no operation that
  prepares an epoch from resident objects. `prepare_revision_impl`
  deliberately accepts a byte-identical refetched pack as preexisting
  (`installed_git_cache.cpp` ~3140-3147, ~3264, ~3419), which is exactly what
  the trace shows.
- **The fetch also re-checks authority.** Its `ls-refs` advertisement checks,
  at import time, that the accepted live base and head are still the
  advertised refs (`git_protocol_v2.hpp` ~241-243). Skipping the fetch drops
  that check for the import.
- **Head equality cannot bind the occurrence to an existing epoch.**
  `record_live_occurrence` refuses to (`epoch_application_host.cpp`
  ~570-580). So does RFC-BPR-0029 (Decisions 1 and 5), and the architecture
  says "only journaled historical completion can establish" an imported
  comparison link (`git-and-epoch-model.md` ~213). The append itself is
  required.
- **The RFC is silent on refetches and haves.** RFC-BPR-0029 says nothing
  about either for imports. `git-and-epoch-model.md` ~455 says only that an
  evidenced after-head "may be fetched" and that import requires verified
  head closure.

### Why offering haves is not a local change either

A server that honours a `have` omits the objects reachable from it. The pack
is then complete but not self-contained. With installed Git 2.53 on this
station, `git index-pack --stdin --strict
--check-self-contained-and-connected` writes such a pack and **exits 1**; it
exits 0 for a self-contained pack and for the empty pack `029d0882…`. The
pinned import argv (`installed_git_process.cpp` ~3855-3866) does not allow
exit 1 (~2376-2384). So a have would:

- make an import of a head equal to a have receive the empty pack (accepted);
- make an import of any other head with resident ancestors, such as
  `6458d96b` here, fail with `process_failure`.

The live path already offers haves and has the same exposure. That is
reported as a new defect, uncarded, in the dsp-003 return. No fixture honours
a have: the public-path fixture transport matches only `want` lines and
serves recorded packs.

## Design question (parked; needs a decision)

> A signed-out historical retention import whose force-push after-head is
> already resident (the live head, or a head an earlier import retained)
> downloads a byte-identical pack and installs nothing. The import cannot run
> without it, because the only journaled import path requires that fetch's
> receipt. Which should the product do?
>
> 1. **Keep the refetch.** Record it as needed and close this card; `:4`
>    stays.
> 2. **Add a resident-object historical import.** A new cache operation would
>    verify commit, closure and derived base under the writer lock with no
>    pack receipt, with a no-spool append journal shape and its recovery. It
>    drops the import-time `ls-refs` check for that occurrence (say whether
>    that is acceptable), and the preview course would pin `:2`. This changes
>    the Git cache port and the append journal.
> 3. **Offer verified haves on historical fetches.** The pinned
>    `index-pack --check-self-contained-and-connected` contract must first
>    accept a non-self-contained pack (allow exit 1 and rely on the existing
>    `verify_connectivity`, or another rule). The acquisition count stays
>    `:4`; the bytes shrink.

Options 2 and 3 are architecture changes, so this station stops here, as
instructed. Option 3 also decides the live-path defect above.
