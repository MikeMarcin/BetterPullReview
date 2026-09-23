---
schema: "implement-rfc.card.v1"
id: "BPR-0889"
title: "The protocol v2 fetch parser's truncated-stream refusal has no visible coverage"
status: "Done"
rfcs: []
depends_on: []
priority: "P3"
platforms: ["mac", "linux", "win"]
metadata: {"found_by":"r5 whole-project critical review, Claude/fable github+git partition; the reviewer flagged this one as uncertain","found_at":"20418bfe","run":"run-20260916-stations","dispatch":"dsp-005"}
acceptance: [{"criterion":"Each truncation shape either has a test or a recorded reason why it cannot occur","outcome":"pass","revision":"cc771b5d","evidence":{"kind":"command","detail":"bpr-main-mac: application_git_protocol_v2 passes test_truncated_fetch_streams. It has eight cuts (empty, mid section header, mid packet header, a whole header with no channel byte, a channel byte with no payload, a channel byte with part of the payload, no flush, half a flush), each fed in one chunk and one byte at a time. It also covers the destructor, a transport that ends cleanly mid-pack, and one that fails mid-pack. The sparse path shares the parser, which is recorded. Negative controls: no abort in finish(), and no abort in the destructor, each fail a new row.","path":"docs/issues/60-done/BPR-0889-no-coverage-for-a-truncated-pack-stream-in-the-protocol-v2-fetch-parser.md#the-rows"}},{"criterion":"A truncated stream leaves no partially written pack behind, and that is asserted rather than assumed","outcome":"pass","revision":"cc771b5d","evidence":{"kind":"command","detail":"git_cache_contract drives the parser into a real pack sink with a stream cut 40 pack bytes in. It asserts the receiving spool holds exactly 40 bytes, and that after finish() refuses neither the receiving spool nor the .pack.part exists while the sink object is still alive, and again after it is destroyed. It passes (268.81 s), and fails with FAIL: the refusal removes the partial pack while the sink still exists when finish() does not abort. Linux and Windows confirmation runs are named on the card.","path":"docs/issues/60-done/BPR-0889-no-coverage-for-a-truncated-pack-stream-in-the-protocol-v2-fetch-parser.md#confirmation-runs-owed-linux-windows"}}]
---

# BPR-0889: The protocol v2 fetch parser's truncated-stream refusal has no visible coverage

## Defect

`src/application/src/git_protocol_v2.cpp:1073-1082` returns
`incomplete_response` from `finish()` when the stream ends mid-packet or
before the flush, and `~git_protocol_v2_fetch_parser` (`:830`) aborts the
sink. Nothing appears to exercise either.

`tests/application/git_protocol_v2_tests.cpp` has five references to failure
reasons and no test named for truncation; grep for
`truncat|mid-packet|partial` returns nothing.

This is a **wire** surface, where a passing suite can still miss the exact
case that matters: bytes stopping right after a sideband channel byte, with
`ChannelRead` true and the payload incomplete. A network that drops mid-pack
is ordinary, and the difference between a clean refusal and a half-consumed
sink is the difference between a retry and a corrupt cache.

The reviewer marked this **uncertain**: it did not read every test body, so
the coverage may exist under a name the grep missed. Confirming that is the
first step and may close the card.

## Scope

- Confirm whether the coverage exists. If it does, record where, and close
  this.
- If it does not, add direct tests that assert `incomplete_response` and that
  the sink observed `abort()`, for at least: truncation mid-packet-header,
  truncation after a sideband channel byte with an incomplete payload, and
  truncation before the flush.
- Check the same question for the sparse path if it has its own parser.

## Acceptance

- Each truncation shape either has a test or a recorded reason why it cannot
  occur.
- A truncated stream leaves no partially written pack behind, and that is
  asserted rather than assumed.

## Every truncation shape has a row, and the real sink is checked (bpr-main-mac, Mac batch 5, 2026-09-23)

Station `bpr-main-mac` (Apple Silicon, AppleClang, Qt 6.11.2, `build/dev`
Debug), claimed at `24104b05`. No product code changed: the parser already
behaved correctly, and this card adds the coverage that shows it.

**Re-check at HEAD: the reviewer's doubt was half right.** One shape was
already covered under a name the grep missed.
`test_fetch_parser_rejections` in `tests/application/git_protocol_v2_tests.cpp`
cuts the last four bytes off the response and asserts `incomplete_response`
and an aborted sink ("missing terminal flush is rejected"). That is truncation
before the flush. Nothing covered a cut inside a packet header, a cut after
the sideband channel byte, the destructor's abort, or what the real sink
leaves on disk.

**The sparse path has no parser of its own.** Complete and sparse acquisition
both call `acquire_git_protocol_v2_pack` (`epoch_application_host.cpp`
~5790-5866). Only `AcquisitionMode` differs. The gh CLI transport builds a
pack with a local `git pack-objects` (with `--filter=blob:none` for sparse),
re-frames it as protocol-v2 sideband packets, and feeds the same
`git_protocol_v2_fetch_parser` (`gh_cli_git_smart_transport.cpp` ~455-518). A
short read of its own spool returns `malformed_response` before the next
`consume`.

### The rows

`test_truncated_fetch_streams` (row `application_git_protocol_v2`) cuts the
complete fixture response (`packfile` header, a progress packet, an
8-byte pack packet, flush) at eight points. It feeds each cut in one chunk and
again one byte at a time. Every run asserts four things: `consume` accepted
everything up to the cut; the sink received exactly the whole pack bytes that
preceded it; `finish()` and `error()` both report `incomplete_response`; and
the sink was aborted exactly once and never finished, with a later `abort()`
not repeating it.

| Shape | Cut | Pack bytes at the sink |
| --- | --- | --- |
| an empty response | 0 bytes | 0 |
| mid section header | 2 bytes into `packfile\n` | 0 |
| mid packet header | 2 of the pack packet's 4 length bytes | 0 |
| a whole packet header and no channel byte | length read, payload not begun | 0 |
| a channel byte and no payload | `ChannelRead` true, 0 payload bytes | 0 |
| a channel byte and part of the payload | `ChannelRead` true, `PAC` of `PACKdata` | 3 |
| a whole pack packet and no flush | the existing shape, now with the exact abort count | 8 |
| half a flush | `00` of `0000` | 8 |

The same function also covers three paths out of a truncated fetch:

- **The destructor.** A parser dropped mid-payload without `finish()` aborts
  its sink once.
- **The public path, with no transport error.** `acquire_git_protocol_v2_pack`
  over a scripted transport whose body stops mid-payload returns
  `git_protocol_v2_error{incomplete_response}` and aborts once. A transport
  can end a body cleanly while the pack is incomplete, and only `finish()`
  can catch that.
- **The public path, with a transport failure mid-pack.** The scripted
  transport gained `FailureAfterBytes`. It fails with `network` after 3 pack
  bytes reached the sink. The acquisition keeps the transport's own failure
  and aborts once.

**The real sink (AC2).** `git_cache_contract` (`git_cache_contract_tests.cpp`
~2897) drives the parser into a real `create_pack_sink` sink with the test's
230-byte SHA-1 pack, framed as one sideband packet and cut 40 pack bytes in.
It checks the receiving spool `.<operation>.pack.receiving` holds exactly
those 40 bytes before `finish()`. After `finish()` refuses, it checks that
neither that spool nor `<operation>.pack.part` exists. **That check runs while
the sink object is still alive**, so it proves the parser's own abort
removes the partial pack, not the sink's destructor. It checks again after
the sink is destroyed.

### Negative controls

Each was a temporary edit, restored afterwards and re-hashed
(`git_protocol_v2.cpp` and the test file match; `grep -c BPR0889_NC` gives 0).

| Break | Result |
| --- | --- |
| `finish()` records `incomplete_response` without aborting the sink | `application_git_protocol_v2` fails. The existing flush row fails first. With that row moved after the new function, the new row fails on its own: `a truncated stream aborts the pack sink once and never finishes it: an empty response, in one chunk`. `git_cache_contract` fails with `FAIL: the refusal removes the partial pack while the sink still exists` |
| The destructor does not abort | `FAILED: a parser abandoned mid-payload aborts its sink when it is destroyed` |

### Verification (macOS)

| Check | Result |
| --- | --- |
| `cmake --build --preset dev` | Exit 0, no `warning:` line |
| `ctest --test-dir build/dev -j1 --output-on-failure -R '^(application_git_protocol_v2\|git_cache_contract)$'` | 2/2 passed: 0.38 s and 268.81 s. `application_git_protocol_v2` passed again, 0.36 s, after the transport-failure case was added |
| clang-format 23.1.1 on both test files | Clean |
| `_coding-standard/scripts/tidy.py` on both test files | No finding on a changed line. The one finding, `git_cache_contract_tests.cpp:1342`, is on an untouched line |

### Confirmation runs owed (Linux, Windows)

The acceptance names no platform. The rows are portable. The real-sink check
uses the same `abort()` cleanup that the existing "failed sink leaves no ...
spool residue" rows already run on every platform. These runs confirm it and
may follow the finish:

- **Linux (bpr-main-linux):** `cmake --build --preset dev && ctest --test-dir build/dev -j1 --output-on-failure -R '^(application_git_protocol_v2|git_cache_contract)$'`
- **Windows (bpr-main-win):** `cmake --build --preset windows-dev && ctest --preset windows-dev -j1 --output-on-failure -R '^(application_git_protocol_v2|git_cache_contract)$'`.
  `git_cache_contract` is registered `WINDOWS_UNCONTAINED`. The spool removal
  after `File.close()` is the Windows-specific part worth watching.
