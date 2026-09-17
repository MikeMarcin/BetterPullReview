---
schema: "implement-rfc.card.v1"
id: "BPR-0819"
title: "The round-trip composition contract pins a github_application.cpp digest from September 13"
status: "Done"
rfcs: ["RFC-BPR-0015"]
depends_on: []
priority: "P2"
platforms: ["mac"]
metadata: {"found_by":"integration run at the BPR-0810/BPR-0811 merge; confirmed at a clean 8b2bea5b worktree","found_at":"8b2bea5b83639c78f69f431e24a724e0fccdb3e9","run":"run-20260916-stations","dispatch":"dsp-001-r2"}
acceptance: [{"criterion":"The contract passes on macOS, and this card lists the reviewed changes the reseal covers","outcome":"pass","revision":"af612a050f9f8a8a74b8307d1f237b8dc7e9b04f plus the reseal committed by this finish","evidence":{"kind":"command","detail":"ctest --test-dir build/r2-dev -R application_review_round_trip_composition_contract: Passed 11.63 s with the resealed pins (build/r2-logs/dsp2-0819-reseal-ctest.log); r2-dev app built at 9767145d, later commits change docs and this script only. Values printed by -DBPR_PRINT_PINNED_DIGESTS=ON; the refactored compute functions reproduce the old pins at ce7df75e and an independent Python computation at HEAD. The card's Reconciliation section lists all 29 pinned-scope commits since ce7df75e with the review covering each, including integrator reviews of c97bb64a and f2b8e682. The async mutation inventory subtest of application_composition_contract, red for the same reason, now passes; two unrelated stale assertions there are carded as BPR-0821."}},{"criterion":"BPR-0788 knows about the reseal","outcome":"pass","revision":"af612a050f9f8a8a74b8307d1f237b8dc7e9b04f plus this finish","evidence":{"kind":"artifact","detail":"docs/issues/10-backlog/BPR-0788-close-rfc-0015.md gains a dated 'Digest reseal' note naming the covered RFC-BPR-0015 commits, the unchanged mutation pin and BPR-0821."}}]
---

# BPR-0819: The round-trip composition contract pins a github_application.cpp digest from September 13

## Defect

`application_review_round_trip_composition_contract`
(`tests/end_to_end/application_review_round_trip_composition_contract.cmake`)
fails in 0.06 s because the source digest it pins for
`src/app/src/github_application.cpp` is
`5df1138b95f54a780a4dbf2aa7312695b9164001f407928b7b08ff1965c15f82`. That value
was last refreshed in `ce7df75e` ("Refresh composition digest for CLI profile
checkpoint", 2026-09-13), in RFC-BPR-0015's first dispatch. It matches none of
the revisions of this run, from `3f1fd775` through `f33a10c9`. The contract has
therefore been red since the file's next change, and a reviewed-digest tripwire
that is always red reviews nothing.

The file changed since then under several cards, each with its own review
record:

- BPR-0786, `df458d30`;
- [[BPR-0797]], `b1f89533` and `a78eefcf`;
- [[BPR-0804]], `f2b8e682`;
- [[BPR-0806]], `d0964fdd`;
- [[BPR-0808]], `282e4908`;
- [[BPR-0810]], `3ca3e12c`.

## Scope

- Read what the digest protects: the review round-trip composition surface
  and the RFC-BPR-0015 confirmation path. Confirm that every change since
  `ce7df75e` to that surface has a recorded review.
- Reseal the digest in the same commit that records that reconciliation, or
  replace the whole-file digest with a narrower check that does not go red on
  unrelated edits to a 13,000-line composition file.
- Do not reseal blindly.

## Acceptance

- The contract passes on macOS, and this card lists the reviewed changes the
  reseal covers.
- [[BPR-0788]] knows about the reseal.

## Reconciliation and reseal (dsp-002, run-20260916-stations)

**Wider than the card said.** Four checks were red, not one: the whole-file
pin, the production manifest pin (which also covers `main.cpp`), and the
source-tree inventory, where [[BPR-0806]]'s `d0964fdd` added the production
header `include/bpr/app/native_filesystem_path.hpp`. The fourth was
`application_composition_contract`'s async-mutation inventory subtest,
which runs the same script. 29 commits changed a pinned scope after
`ce7df75e`, not 7. The narrow mutation-branch pin (`a7e2081e…`) matched at
every commit, so no commit touched the tests-disabled production branch of
`compose_review_mutation`.

**Reseal, not narrowing, and why.** The whole-file digest is a documented
claim of closed RFCs: `docs/architecture/system-architecture.md` ("A full
LF-normalized digest locks the composition translation unit") and the
RFC-BPR-0008 verification row that cites it. RFC-BPR-0012 cites it as well.
Narrowing it would amend those records, which is a change beyond this card.
This card resealed the pins, and commit `5f94c668` removed the reason they
went stale silently:

- the three values come from shared compute functions;
- `-DBPR_PRINT_PINNED_DIGESTS=ON` prints them without verifying anything;
- the reseal procedure sits next to the pins.

A narrower tripwire remains a candidate for the RFC-BPR-0008 owner. It is not
done here.

**The reviews this reseal covers.** A token audit scanned the pinned-scope
hunks for publish, submit, mutation, confirm, credential, vault, grant,
authority, CLI, POST, token, consent and approval. It left only the rows
below as request- or authority-relevant.
`scratchpad/int-0819/audit.py` is the audit script, not committed.

| Commit | Card | Review that covers it |
|---|---|---|
| `cdf6175c` CLI reads to bounded Git objects | BPR-0785 | `docs/verification/RFC-BPR-0015.md`; [[BPR-0788]] checklist, independent authority/protocol review adjudicated |
| `df458d30` guarded CLI review and inline publication | BPR-0786 | the same, plus the BPR-0786 card |
| `b1f89533`, `a78eefcf` run-time suite question, CLI consent gates | [[BPR-0797]] | BPR-0797's independent critical review |
| `c97bb64a` sparse current epochs bound to exact blobs | none | **Integrator review here.** The app adds `supports_sparse_commit_trees() -> true` to the CLI Git transport. The transport rejects a non-integral blob size as malformed, and reports an oversize one as `response_limit` instead of malformed. Read-only; no new request, credential or mutation edge. |
| `f2b8e682` name the CLI publication operation in every graph | [[BPR-0804]] | **Integrator review here.** `static_cast<void>(aOperation)` moves above `#if defined(BPR_BUILD_TESTING)`. Compile-only; the prompt and return paths are unchanged. |
| `d0964fdd` Windows profile desktop data root | [[BPR-0806]] | BPR-0806's independent critical review (no blocker). Pinned-file effect: the shared `native_filesystem_path` replaces two local copies. Bytes are unchanged off Windows; Windows gets native separators. |

The other rows changed presentation or course code only. Their token hits,
where there were any, are Dif context identity, CLI-row course expectations
and a restore notice:

- RFC-BPR-0031 Dif work: `6f07c343`, `e337c1fa`, `4bbecb48`, `3a009291`,
  `8353a4fe`, `8512a3d8`, `5fe8045e`, recorded in the RFC-BPR-0031
  verification records;
- Home and accounts: `284d1eac`, `aee19b26`, `51c443d2`, `2729b93a`,
  `ce62ed52`, recorded on the BPR-0781 closeout;
- the profile chooser and menu bar: `48aa547f`, `21a2add7`, `1c40d04b`, on
  the BPR-0782 closeout;
- appearance: `867e4bed`, `a5427499`, on the BPR-0791 closeout;
- course code: `1bd5c5e8`, a `main.cpp` course tooltip check; `67691f81`,
  [[BPR-0815]]'s course helper;
- presentation: `282e4908`, [[BPR-0808]]'s workspace-repair refusal display;
  `3ca3e12c`, [[BPR-0810]]'s file-receipt display after a same-file line
  selection. Neither publishes or requests anything.

New values at `af612a05`, printed by the new mode:

- whole file `404bbe3a92fb8aa9aae16190c44b42f758c814ef14f5be461bd1ccac4e636817`;
- manifest `2385725a4803c0eb77a8bde37040d692c2d3c873aa5f659ebb9c81eda570155f`;
- mutation branch unchanged.

**Result.** `application_review_round_trip_composition_contract` passes on
macOS in 11.63 s. The async-mutation inventory subtest of
`application_composition_contract` passes too
(`build/r2-logs/dsp2-0819-reseal-ctest.log`).

That same run has two other composition failures. Both predate this card and
are not caused by it:

- `test_concrete_github_selection_is_owned_by_composition` still expects
  `#if defined(__APPLE__) && !defined(BPR_BUILD_TESTING)`, a gate BPR-0797's
  `a78eefcf` replaced;
- `test_tests_off_release_graph_omits_public_path_fixture` finds
  `BPR_BUILD_TESTING` in `github_application.hpp`, which `b1f89533` added.

[[BPR-0821]] owns both.
